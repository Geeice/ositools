#include "stdafx.h"
#include "OsirisProxy.h"
#include "NodeHooks.h"
#include "DebugInterface.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <psapi.h>

namespace osidbg
{

std::unique_ptr<OsirisProxy> gOsirisProxy;

uint8_t * ResolveRealFunctionAddress(uint8_t * Address)
{
	// Resolve function pointer through relocations
	for (uint8_t * ptr = Address; ptr < Address + 64; ptr++)
	{
		// Look for the instruction "cmp qword ptr [rip+xxxxxx], 0"
		if (ptr[0] == 0x48 && ptr[1] == 0x83 && ptr[2] == 0x3d && ptr[6] == 0x00 &&
			// Look for the instruction "jmp xxxx"
			ptr[13] == 0xe9)
		{
			int32_t relOffset = *reinterpret_cast<int32_t *>(ptr + 14);
			return ptr + relOffset + 18;
		}
	}

	// Could not find any relocations
	return Address;
}

void * OsirisProxy::FindRuleActionCallProc()
{
#if 0
	Debug(L"OsirisProxy::FindRuleActionCallProc");
#endif
	uint8_t * Addr = static_cast<uint8_t *>(OsirisDllStart);

	// Function prologue of RuleAction::Call
	static const uint8_t instructions[18] = {
		0x40, 0x55, // push rbp
		0x53, // push rbx
		0x56, // push rsi
		0x41, 0x56, // push r14
		0x48, 0x8D, 0x6C, 0x24, 0xC1, // lea rbp, [rsp-3Fh]
		0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00 // sub rsp, 88h
	};

	// Look for prologue in the entire osiris DLL
	for (uint8_t * ptr = Addr; ptr < Addr + OsirisDllSize; ptr++)
	{
		if (*reinterpret_cast<uint64_t *>(ptr) == *reinterpret_cast<uint64_t const *>(&instructions[0])
			&& memcmp(instructions, ptr, sizeof(instructions)) == 0)
		{
			return ptr;
		}
	}

	return nullptr;
}

void OsirisProxy::FindOsirisGlobals(FARPROC CtorProc)
{
#if 0
	Debug(L"OsirisProxy::FindOsirisGlobals:");
#endif
	uint8_t * Addr = ResolveRealFunctionAddress((uint8_t *)CtorProc);

	// Try to find pointers of Osiris globals
	const unsigned NumGlobals = 8;
	uint8_t * globals[NumGlobals];
	unsigned foundGlobals = 0;
	for (uint8_t * ptr = Addr; ptr < Addr + 0x300; ptr++)
	{
		// Look for the instruction "jmp short $+7"
		if (ptr[0] == 0xEB && ptr[1] == 0x07 &&
			// Look for the instruction "mov cs:[rip + xxx], <64-bit register>"
			ptr[2] == 0x48 && ptr[3] == 0x89 && (ptr[4] & 0xC7) == 0x05)
		{
			int32_t relOffset = *reinterpret_cast<int32_t *>(ptr + 5);
			uint64_t osiPtr = (uint64_t)ptr + relOffset + 9;
			globals[foundGlobals++] = (uint8_t *)osiPtr;
			if (foundGlobals == NumGlobals) break;
		}
	}

	if (foundGlobals < NumGlobals)
	{
		Fail(L"Could not locate global Osiris variables");
	}

	Globals.Variables = (VariableDb **)globals[0];
	Globals.Types = (TypeDb **)globals[1];
	Globals.Functions = (TypeDb **)globals[2];
	Globals.Objects = (TypeDb **)globals[3];
	Globals.Goals = (void **)globals[4];
	Globals.Adapters = (AdapterDb **)globals[5];
	Globals.Databases = (DatabaseDb **)globals[6];
	Globals.Nodes = (NodeDb **)globals[7];

#if 0
	Debug(L"\tVariables = %p", Globals.Variables);
	Debug(L"\tTypes = %p", Globals.Types);
	Debug(L"\tFunctions = %p", Globals.Functions);
	Debug(L"\tObjects = %p", Globals.Objects);
	Debug(L"\tGoals = %p", Globals.Goals);
	Debug(L"\tAdapters = %p", Globals.Adapters);
	Debug(L"\tDatabases = %p", Globals.Databases);
	Debug(L"\tNodes = %p", Globals.Nodes);
#endif
}

void OsirisProxy::FindDebugFlags(FARPROC SetOptionProc)
{
	uint8_t * Addr = ResolveRealFunctionAddress((uint8_t *)SetOptionProc);

	// Try to find pointer of global var DebugFlags
	for (uint8_t * ptr = Addr; ptr < Addr + 0x80; ptr++)
	{
		// Look for the instruction "mov ecx, cs:xxx"
		if (ptr[0] == 0x8B && ptr[1] == 0x0D &&
			// Look for the instruction "shr e*x, 14h"
			ptr[8] == 0xC1 && ptr[10] == 0x14)
		{
			int32_t relOffset = *reinterpret_cast<int32_t *>(ptr + 2);
			uint64_t dbgPtr = (uint64_t)ptr + relOffset + 6;
			Globals.DebugFlags = (DebugFlag *)dbgPtr;
			break;
		}
	}

	if (Globals.DebugFlags == nullptr) {
		Fail(L"Could not locate global variable DebugFlags");
	}

#if 0
	Debug(L"OsirisProxy::FindDebugFlags: DebugFlags = %p", Globals.DebugFlags);
#endif
}

void OsirisProxy::Initialize()
{
	Debug(L"OsirisProxy::Initialize: Starting");
	OsirisModule = LoadLibrary(L"osiris_x64.dll");
	if (OsirisModule == NULL) {
		Fail(L"Could not load osiris_x64.dll");
	}

	MODULEINFO moduleInfo;
	if (!GetModuleInformation(GetCurrentProcess(), OsirisModule, &moduleInfo, sizeof(moduleInfo))) {
		Fail(L"Could not get module info of osiris_x64.dll");
	}

	OsirisDllStart = moduleInfo.lpBaseOfDll;
	OsirisDllSize = moduleInfo.SizeOfImage;

	FARPROC OsirisCtorProc = GetProcAddress(OsirisModule, "??0COsiris@@QEAA@XZ");
	if (OsirisCtorProc == NULL) {
		Fail(L"Could not locate COsiris::COsiris() in osiris_x64.dll");
	}

	FindOsirisGlobals(OsirisCtorProc);

	OriginalRuleActionCallProc = (RuleActionCallProc)FindRuleActionCallProc();
	if (!OriginalRuleActionCallProc) {
		Fail(L"Could not locate RuleAction::Call in osiris_x64.dll");
	}

	FARPROC RegisterDIVFunctionsProc = GetProcAddress(OsirisModule, "?RegisterDIVFunctions@COsiris@@QEAAXPEAUTOsirisInitFunction@@@Z");
	if (RegisterDIVFunctionsProc == NULL) {
		Fail(L"Could not locate RegisterDIVFunctions in osiris_x64.dll");
	}

#if 0
	Debug(L"OsirisProxy::Initialize: Detouring RegisterDIVFunctions");
#endif
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	RegisterDivFunctions.Wrap(RegisterDIVFunctionsProc, &SRegisterDIVFunctionsWrapper);
	DetourTransactionCommit();

	FARPROC SetOptionProc = GetProcAddress(OsirisModule, "?SetOption@COsiris@@QEAAXI@Z");
	if (SetOptionProc == NULL) {
		Fail(L"Could not locate COsiris::SetOption in osiris_x64.dll");
	}

	FindDebugFlags(SetOptionProc);

	FARPROC OpenLogFileProc = GetProcAddress(OsirisModule, "?OpenLogFile@COsiris@@QEAA_NPEB_W0@Z");
	if (OpenLogFileProc == NULL) {
		Fail(L"Could not locate COsiris::OpenLogFile in osiris_x64.dll");
	}

	OsirisOpenLogFileProc = (COsirisOpenLogFileProc)OpenLogFileProc;

	FARPROC CloseLogFileProc = GetProcAddress(OsirisModule, "?CloseLogFile@COsiris@@QEAAXXZ");
	if (CloseLogFileProc == NULL) {
		Fail(L"Could not locate COsiris::CloseLogFile in osiris_x64.dll");
	}

	OsirisCloseLogFileProc = (COsirisCloseLogFileProc)CloseLogFileProc;

	FARPROC ReadHeaderProc = GetProcAddress(OsirisModule, "?_ReadHeader@COsiris@@IEAA_NAEAVCOsiSmartBuf@@AEAE111PEADAEAI@Z");
	if (ReadHeaderProc == NULL) {
		Fail(L"Could not locate COsiris::_ReadHeader in osiris_x64.dll");
	}

	OsirisReadHeaderProc = (COsirisReadHeaderProc)ReadHeaderProc;

	FARPROC LoadProc = GetProcAddress(OsirisModule, "?Load@COsiris@@QEAA_NAEAVCOsiSmartBuf@@@Z");
	if (LoadProc == NULL) {
		Fail(L"Could not locate COsiris::Load in osiris_x64.dll");
	}

	OsirisLoadProc = (COsirisLoadProc)LoadProc;
}

void OsirisProxy::Shutdown()
{
	Debug(L"OsirisProxy::Shutdown: Starting");
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	WrappedRuleActionCall.Unwrap();
	RegisterDivFunctions.Unwrap();
	WrappedOsirisReadHeader.Unwrap();
	DetourTransactionCommit();

	FreeLibrary(OsirisModule);
}

void OsirisProxy::EnableHooks(bool Enabled)
{
	HooksEnabled = Enabled;
}

void OsirisProxy::SetupLogging(bool Enabled, DebugFlag LogLevel, std::wstring const & Path)
{
	DeleteFileW(Path.c_str());
	LoggingEnabled = Enabled;
	DesiredLogLevel = LogLevel;
	LogDirectory = Path;
}

void OsirisProxy::RestartLogging()
{
	DebugFlag NewFlags = (DebugFlag)((DesiredLogLevel & 0xffff0000) | (*Globals.DebugFlags & 0x0000ffff));

	if (LogFilename.empty())
	{
		BOOL created = CreateDirectoryW(LogDirectory.c_str(), NULL);
		if (created == FALSE)
		{
			DWORD lastError = GetLastError();
			if (lastError != ERROR_ALREADY_EXISTS)
			{
				std::wstringstream err;
				err << L"Could not create log directory '" << LogDirectory << "': Error " << lastError;
				Fail(err.str().c_str());
			}
		}

		auto now = std::chrono::system_clock::now();
		auto tt = std::chrono::system_clock::to_time_t(now);
		std::tm tm;
		gmtime_s(&tm, &tt);

		std::wstringstream ss;
		ss  << std::setfill(L'0') 
			<< (tm.tm_year + 1900) << "-" 
			<< std::setw(2) << (tm.tm_mon + 1) << "-"
			<< std::setw(2) << tm.tm_mday << " "
			<< std::setw(2) << tm.tm_hour << "-"
			<< std::setw(2) << tm.tm_min << "-"
			<< std::setw(2) << tm.tm_sec << ".log";
		LogFilename = LogDirectory + L"\\" + ss.str();

		Debug(L"OsirisProxy::RestartLogging: Starting debug logging.\r\n"
			"\tPath=%s\r\n"
			"\tOriginalDebugFlags=%08x\r\n"
			"\tDebugFlags=%08x", LogFilename.c_str(), *Globals.DebugFlags, NewFlags);
	}

	OsirisCloseLogFileProc(Globals.OsirisObject);
	*Globals.DebugFlags = NewFlags;
	OsirisOpenLogFileProc(Globals.OsirisObject, LogFilename.c_str(), L"ab+");
}

void OsirisProxy::HookNodeVMTs()
{
	gNodeVMTWrappers = std::make_unique<NodeVMTWrappers>(NodeVMTs);
}

void DebugThreadRunner(DebugInterface & intf)
{
	intf.Run();
}

int OsirisProxy::RegisterDIVFunctionsWrapper(void * Osiris, DivFunctions * Functions)
{
	StoryLoaded = false;
	Globals.OsirisObject = Osiris;
	uint8_t * interfaceLoadPtr = nullptr;
	uint8_t * errorMessageFunc = ResolveRealFunctionAddress((uint8_t *)Functions->ErrorMessage);

	// Try to find ptr of gOsirisInterface
	OsirisInterface * osirisInterface = nullptr;
	for (uint8_t * ptr = errorMessageFunc; ptr < errorMessageFunc + 64; ptr++) {
		// Look for the instruction "mov rbx, cs:gOsirisInterface"
		if (ptr[0] == 0x48 && ptr[1] == 0x8B && ptr[2] == 0x1D && ptr[6] < 0x02) {
			int32_t relOffset = *reinterpret_cast<int32_t *>(ptr + 3);
			uint64_t osiPtr = (uint64_t)ptr + relOffset + 7;
			osirisInterface = *(OsirisInterface **)osiPtr;
			Globals.Manager = osirisInterface->Manager;
			break;
		}
	}

	if (Globals.Manager == nullptr) {
		Fail(L"Could not locate OsirisInterface");
	}

	if (HooksEnabled) {
		CallOriginal = Functions->Call;
		QueryOriginal = Functions->Query;
		ErrorOriginal = Functions->ErrorMessage;
		AssertOriginal = Functions->Assert;

		Functions->Call = &SCallWrapper;
		Functions->Query = &SQueryWrapper;
		Functions->ErrorMessage = &SErrorWrapper;
		Functions->Assert = &SAssertWrapper;
	}

	if (!WrappedOsirisReadHeader.IsWrapped()) {
#if 0
		Debug(L"OsirisProxy::RegisterDIVFunctionsWrapper: Hooking COsiris functions");
#endif
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		WrappedOsirisReadHeader.Wrap(OsirisReadHeaderProc, &SOsirisReadHeader);
		WrappedOsirisLoad.Wrap(OsirisLoadProc, &SOsirisLoad);
		WrappedRuleActionCall.Wrap(OriginalRuleActionCallProc, &SRuleActionCall);
		DetourTransactionCommit();
	}

	if (LoggingEnabled) {
		RestartLogging();
	}

#if 0
	Debug(L"OsirisProxy::RegisterDIVFunctionsWrapper: Initializing story.");
	Debug(L"\tErrorMessageProc = %p", errorMessageFunc);
	Debug(L"\tOsirisManager = %p", Globals.Manager);
	Debug(L"\tOsirisInterface = %p", osirisInterface);
#endif

	if (DebuggerThread == nullptr) {
		debugInterface_ = std::make_unique<DebugInterface>();
		debugMsgHandler_ = std::make_unique<DebugMessageHandler>(std::ref(*debugInterface_));

		DebuggerThread = new std::thread(std::bind(DebugThreadRunner, std::ref(*debugInterface_)));
	}

	return RegisterDivFunctions(Osiris, Functions);
}

int OsirisProxy::SRegisterDIVFunctionsWrapper(void * Osiris, DivFunctions * Functions)
{
	return gOsirisProxy->RegisterDIVFunctionsWrapper(Osiris, Functions);
}

bool OsirisProxy::CallWrapper(uint32_t FunctionId, CallParam * Params)
{
	// DebugDumpCall("Call", FunctionId, Params);
	return CallOriginal(FunctionId, Params);
}

bool OsirisProxy::SCallWrapper(uint32_t FunctionId, CallParam * Params)
{
	return gOsirisProxy->CallWrapper(FunctionId, Params);
}

bool OsirisProxy::QueryWrapper(uint32_t FunctionId, CallParam * Params)
{
	// DebugDumpCall("Query", FunctionId, Params);
	return QueryOriginal(FunctionId, Params);
}

bool OsirisProxy::SQueryWrapper(uint32_t FunctionId, CallParam * Params)
{
	return gOsirisProxy->QueryWrapper(FunctionId, Params);
}

void OsirisProxy::ErrorWrapper(char const * Message)
{
	ErrorOriginal(Message);
}

void OsirisProxy::SErrorWrapper(char const * Message)
{
	gOsirisProxy->ErrorWrapper(Message);
}

void OsirisProxy::AssertWrapper(bool Successful, char const * Message, bool Unknown2)
{
	if (!Successful) {
		Debug("Osiris Assert: %s", Message);
	}

	AssertOriginal(Successful, Message, Unknown2);
}

void OsirisProxy::SAssertWrapper(bool Successful, char const * Message, bool Unknown2)
{
	gOsirisProxy->AssertWrapper(Successful, Message, Unknown2);
}

int OsirisProxy::OsirisReadHeader(void * Osiris, void * OsiSmartBuf, unsigned __int8 * MajorVersion, unsigned __int8 * MinorVersion, unsigned __int8 * BigEndian, unsigned __int8 * Unused, char * StoryFileVersion, unsigned int * DebugFlags)
{
	int retval = WrappedOsirisReadHeader(Osiris, OsiSmartBuf, MajorVersion, MinorVersion, BigEndian, Unused, StoryFileVersion, DebugFlags);
	if (LoggingEnabled) {
		uint32_t NewFlags = (DesiredLogLevel & 0xffff0000) | (*DebugFlags & 0x0000ffff);
#if 0
		Debug(L"OsirisProxy::OsirisReadHeader: Change DebugFlags from %08x to %08x", *DebugFlags, NewFlags);
#endif
		*DebugFlags = NewFlags;
	}

	return retval;
}

int OsirisProxy::SOsirisReadHeader(void * Osiris, void * OsiSmartBuf, unsigned __int8 * MajorVersion, unsigned __int8 * MinorVersion, unsigned __int8 * BigEndian, unsigned __int8 * Unused, char * StoryFileVersion, unsigned int * DebugFlags)
{
	return gOsirisProxy->OsirisReadHeader(Osiris, OsiSmartBuf, MajorVersion, MinorVersion, BigEndian, Unused, StoryFileVersion, DebugFlags);
}

int OsirisProxy::OsirisLoad(void * Osiris, void * Buf)
{
	int retval = WrappedOsirisLoad(Osiris, Buf);

	if (!ResolvedNodeVMTs) {
		ResolveNodeVMTs(*Globals.Nodes);
		ResolvedNodeVMTs = true;
		HookNodeVMTs();
	}

	StoryLoaded = true;
	Debug(L"OsirisProxy::Load: %d nodes", (*Globals.Nodes)->Db.Size);

	if (DebuggerThread != nullptr && gNodeVMTWrappers) {
		debugger_.reset();
		debugger_ = std::make_unique<Debugger>(Globals, std::ref(*debugMsgHandler_));
		debugger_->StoryLoaded();
	}

	return retval;
}

int OsirisProxy::SOsirisLoad(void * Osiris, void * Buf)
{
	return gOsirisProxy->OsirisLoad(Osiris, Buf);
}

void OsirisProxy::RuleActionCall(RuleActionNode * Action, void * a1, void * a2, void * a3, void * a4)
{
	if (debugger_ != nullptr) {
		debugger_->RuleActionPreHook(Action);
	}

	WrappedRuleActionCall(Action, a1, a2, a3, a4);

	if (debugger_ != nullptr) {
		debugger_->RuleActionPostHook(Action);
	}
}

void OsirisProxy::SRuleActionCall(RuleActionNode * Action, void * a1, void * a2, void * a3, void * a4)
{
	gOsirisProxy->RuleActionCall(Action, a1, a2, a3, a4);
}

void DebugDumpParam(std::wstringstream & ss, CallParam & param)
{
	switch (param.Type) {
	case ValueType::Integer:
		ss << param.Value.Int;
		break;

	case ValueType::Integer64:
		ss << param.Value.Int64;
		break;

	case ValueType::Real:
		ss << param.Value.Real;
		break;

	case ValueType::None:
	case ValueType::Undefined:
		ss << "(None)";
		break;

	default:
		if (param.Value.String) {
			ss << '"' << param.Value.String << '"';
		} else {
			ss << "(Null)";
		}
		break;
	}
}

void OsirisProxy::DebugDumpCall(char const * Type, OsirisFunctionHandle Handle, CallParam * Params)
{
	Node * firstNode = *(*Globals.Nodes)->Db.Start;
	Database * db = (*Globals.Databases)->Db.Start[4];
	auto * node = &db->Facts.Head;
	std::wstringstream ss;

	auto functionId = Handle.GetFunctionId();
	auto function = Globals.Manager->Functions.Buffer[functionId];
	ss << "[" << Type << "] " << function->Name << " (";

	bool hasOutputs = false;
	auto param = Params;
	unsigned paramNo = 0;
	while (param) {
		auto & arg = function->Arguments[paramNo];
		if (arg.Direction == FunctionArgumentDirection::In) {
			if (paramNo > 0) {
				ss << ", ";
			}
			ss << arg.Name << ": ";
			DebugDumpParam(ss, *param);
		} else {
			hasOutputs = true;
		}

		param = param->NextParam;
		paramNo++;
	}
	ss << ")";

	if (hasOutputs) {
		ss << " -> (";
		param = Params;
		paramNo = 0;
		while (param) {
			auto & arg = function->Arguments[paramNo];
			if (arg.Direction == FunctionArgumentDirection::Out) {
				ss << arg.Name << ": ";
				DebugDumpParam(ss, *param);
				if (param->NextParam) {
					ss << ", ";
				}
			}

			param = param->NextParam;
			paramNo++;
		}
		ss << ")";
	}
	
	ss << std::endl;

	OutputDebugStringW(ss.str().c_str());
}


void OsirisProxy::ResolveNodeVMTs(NodeDb * Db)
{
#if 0
	Debug(L"OsirisProxy::ResolveNodeVMTs");
#endif
	std::set<NodeVMT *> VMTs;

	for (unsigned i = 0; i < Db->Db.Size; i++) {
		auto node = Db->Db.Start[i];
		auto vmt = *(NodeVMT **)node;
		VMTs.insert(vmt);
	}

	if (VMTs.size() != (unsigned)NodeType::Max + 1) {
		Fail(L"Could not locate all Osiris node VMT-s");
	}

	// RuleNode has a different SetLineNumber implementation
	void * srv{ nullptr };
	std::vector<NodeVMT *> srvA, srvB;
	for (auto vmt : VMTs) {
		if (srv == nullptr) {
			srv = vmt->SetLineNumber;
		}

		if (srv == vmt->SetLineNumber) {
			srvA.push_back(vmt);
		} else {
			srvB.push_back(vmt);
		}
	}

	NodeVMT * ruleNodeVMT;
	if (srvA.size() == 1) {
		ruleNodeVMT = *srvA.begin();
	} else if (srvB.size() == 1) {
		ruleNodeVMT = *srvB.begin();
	} else {
		Fail(L"Could not locate RuleNode::__vfptr");
	}

#if 0
	Debug(L"RuleNode::__vfptr is %p", ruleNodeVMT);
#endif
	NodeVMTs[(unsigned)NodeType::Rule] = ruleNodeVMT;

	// RelOpNode is the only node that has the same GetAdapter implementation
	NodeVMT * relOpNodeVMT{ nullptr };
	for (auto vmt : VMTs) {
		if (vmt->GetAdapter == ruleNodeVMT->GetAdapter
			&& vmt != ruleNodeVMT) {
			if (relOpNodeVMT == nullptr) {
				relOpNodeVMT = vmt;
			} else {
				Fail(L"RelOpNode::__vfptr pattern matches multiple VMT-s");
			}
		}
	}

	if (relOpNodeVMT == nullptr) {
		Fail(L"Could not locate RelOpNode::__vfptr");
	}

#if 0
	Debug(L"RuleNode::__vfptr is %p", relOpNodeVMT);
#endif
	NodeVMTs[(unsigned)NodeType::RelOp] = relOpNodeVMT;

	// Find And, NotAnd
	NodeVMT * and1VMT{ nullptr }, *and2VMT{ nullptr };
	for (auto vmt : VMTs) {
		if (vmt->SetNextNode == ruleNodeVMT->SetNextNode
			&& vmt->GetAdapter != ruleNodeVMT->GetAdapter) {
			if (and1VMT == nullptr) {
				and1VMT = vmt;
			} else if (and2VMT == nullptr) {
				and2VMT = vmt;
			} else {
				Fail(L"AndNode::__vfptr pattern matches multiple VMT-s");
			}
		}
	}

	if (and1VMT == nullptr || and2VMT == nullptr) {
		Fail(L"Could not locate AndNode::__vfptr");
	}

#if 0
	Debug(L"AndNode::__vfptr is %p and %p", and1VMT, and2VMT);
#endif
	// No reliable way to detect these; assume that AndNode VMT < NotAndNode VMT
	if (and1VMT < and2VMT) {
		NodeVMTs[(unsigned)NodeType::And] = and1VMT;
		NodeVMTs[(unsigned)NodeType::NotAnd] = and2VMT;
	} else {
		NodeVMTs[(unsigned)NodeType::NotAnd] = and1VMT;
		NodeVMTs[(unsigned)NodeType::And] = and2VMT;
	}

	// Find Query nodes
	void * snn{ nullptr };
	std::map<void *, std::vector<NodeVMT *>> snnMaps;
	std::vector<NodeVMT *> * queryVMTs{ nullptr };
	for (auto vmt : VMTs) {
		if (snnMaps.find(vmt->SetNextNode) == snnMaps.end()) {
			std::vector<NodeVMT *> nvmts{ vmt };
			snnMaps.insert(std::make_pair(vmt->SetNextNode, nvmts));
		} else {
			snnMaps[vmt->SetNextNode].push_back(vmt);
		}
	}

	for (auto & map : snnMaps) {
		if (map.second.size() == 3) {
			queryVMTs = &map.second;
			break;
		}
	}

	if (queryVMTs == nullptr) {
		Fail(L"Could not locate all Query node VMT-s");
	}

	for (auto vmt : *queryVMTs) {
		auto getName = (NodeVMT::GetQueryNameProc)vmt->GetQueryName;
		std::string name{ getName(nullptr) };
		if (name == "internal query") {
#if 0
			Debug(L"InternalQuery::__vfptr is %p", vmt);
#endif
			NodeVMTs[(unsigned)NodeType::InternalQuery] = vmt;
		} else if (name == "DIV query") {
#if 0
			Debug(L"DivQuery::__vfptr is %p", vmt);
#endif
			NodeVMTs[(unsigned)NodeType::DivQuery] = vmt;
		} else if (name == "Osi user query") {
#if 0
			Debug(L"UserQuery::__vfptr is %p", vmt);
#endif
			NodeVMTs[(unsigned)NodeType::UserQuery] = vmt;
		} else {
			Fail(L"Unrecognized Query node VMT");
		}
	}


	// Proc node has different IsProc() code
	NodeVMT * procNodeVMT{ nullptr }, * databaseNodeVMT{ nullptr };
	for (auto vmt : VMTs) {
		if (vmt->IsProc != ruleNodeVMT->IsProc) {
			if (procNodeVMT == nullptr) {
				procNodeVMT = vmt;
			} else {
				Fail(L"ProcNode::__vfptr pattern matches multiple VMT-s");
			}
		}

		if (vmt->IsDataNode != ruleNodeVMT->IsDataNode
			&& vmt->IsProc == ruleNodeVMT->IsProc) {
			if (databaseNodeVMT == nullptr) {
				databaseNodeVMT = vmt;
			} else {
				Fail(L"DatabaseNode::__vfptr pattern matches multiple VMT-s");
			}
		}
	}

	if (procNodeVMT == nullptr) {
		Fail(L"Could not locate ProcNode::__vfptr");
	}

	if (databaseNodeVMT == nullptr) {
		Fail(L"Could not locate DatabaseNode::__vfptr");
	}

#if 0
	Debug(L"ProcNode::__vfptr is %p", procNodeVMT);
	Debug(L"DatabaseNode::__vfptr is %p", databaseNodeVMT);
#endif
	NodeVMTs[(unsigned)NodeType::Proc] = procNodeVMT;
	NodeVMTs[(unsigned)NodeType::Database] = databaseNodeVMT;
}

}