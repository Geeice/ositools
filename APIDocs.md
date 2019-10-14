### !!! DRAFT API DOCS !!!

### Notes on API stability
Functions without any *TODO* or *Stability* comments are considered to be final and will not change. (Minor behavior changes or bugfixes may happen, but the API will not break.)
Functions marked *Experimental* might be removed in future versions, or may change without any restriction.

# Stat functions

These functions can be used to query stats entries.

### StatExists
`query NRD_StatExists([in](STRING)_StatsId)`

Checks whether the specified stat entry exists. The query succeeds if the stat entry exists, and fails if it does not.

### StatAttributeExists
`query NRD_StatAttributeExists([in](STRING)_StatsId, [in](STRING)_Attribute)`

Checks whether the stat entry `_StatsId` has an attribute (data) named `_Attribute`. The query succeeds if the attribute exists, and fails if it does not.

### StatGetAttributeInt
`query NRD_StatGetAttributeInt([in](STRING)_StatsId, [in](STRING)_Attribute, [out](INTEGER)_Value)`

Returns the specified `_Attribute` of the stat entry.
If the stat entry does not exist, the stat entry doesn't have an attribute named `_Attribute`, or the attribute isn't convertible to integer, the query fails.

**Notes:**
 - For enumerations, the function will return the index of the value in the enumeration. eg. for Damage Type `Corrosive`, it will return 3.

### StatGetAttributeString
`query NRD_StatGetAttributeString([in](STRING)_StatsId, [in](STRING)_Attribute, [out](STRING)_Value)`

Returns the specified `_Attribute` of the stat entry.
If the stat entry does not exist, the stat entry doesn't have an attribute named `_Attribute`, or the attribute isn't convertible to string, the query fails.

**Notes:**
 - For enumerations, the function will return the name of the enumeration value (eg. `Corrosive`).

### StatGetType
`query NRD_StatGetType([in](STRING)_StatsId, [out](STRING)_Type)`

Returns the type of the specified stat entry. If the stat entry does not exist, the query fails.
Possible return values: `Character`, `Potion`, `Armor`, `Object`, `Shield`, `Weapon`, `SkillData`, `StatusData`.


# Status functions

### IterateCharacterStatuses
`call NRD_IterateCharacterStatuses((GUIDSTRING)_CharacterGuid, (STRING)_Event)`
`event NRD_StatusIteratorEvent((STRING)_Event, (CHARACTERGUID)_Character, (STRING)_StatusId, (INTEGER64)_StatusHandle)`

Throws status iterator event `_Event` for each status present on the character. Unlike regular events, `NRD_StatusIteratorEvent` events are not queued and are thrown immediately (i.e. during the `NRD_IterateCharacterStatuses` call), so there is no need for an additional cleanup/finalizer event.

Example usage:  
```c
// ...
NRD_IterateCharacterStatuses(Sandbox_Market_Ernest_Herringway_da8d55ba-0855-4147-b706-46bbc67ec8b6, "MyMod_Statuses");

IF
NRD_StatusIteratorEvent("MyMod_Statuses", _Char, _StatusId, _StatusIdx)
THEN
DebugBreak(_StatusId);
```

**Notes:**
 - The call might not enumerate all statuses correctly (i.e. miss some statuses or iterate over them multiple times) if statuses are added or removed during the `NRD_StatusIteratorEvent` event
 - The `_StatusHandle` is a persistent, unique value (like UUIDs) identifying the status instance. Unlike status names, it can be used to differentiate between two instances of the same status.

### StatusGetHandle
`query NRD_StatusGetHandle([in](GUIDSTRING)_Character, [in](STRING)_StatusId, [out](INTEGER64)_StatusHandle)`

Returns the handle of the first status with the specified `_StatusId`. If no such status exists, the query fails.

The `_StatusHandle` is a persistent, unique value (like UUIDs) identifying the status instance.

Example usage:
```c
// ...
AND
NRD_StatusGetHandle(Sandbox_Market_Ernest_Herringway_da8d55ba-0855-4147-b706-46bbc67ec8b6, "WET", _Handle)
THEN
NRD_DebugLog((STRING)_Handle);
```

### StatusGet
`query NRD_StatusGetInt([in](GUIDSTRING)_Character, [in](INTEGER64)_StatusHandle, [in](STRING)_Attribute, [out](INTEGER)_Value)`
`query NRD_StatusGetReal([in](GUIDSTRING)_Character, [in](INTEGER64)_StatusHandle, [in](STRING)_Attribute, [out](REAL)_Value)`
`query NRD_StatusGetString([in](GUIDSTRING)_Character, [in](INTEGER64)_StatusHandle, [in](STRING)_Attribute, [out](STRING)_Value)`
`query NRD_StatusGetGuidString([in](GUIDSTRING)_Character, [in](INTEGER64)_StatusHandle, [in](STRING)_Attribute, [out](GUIDSTRING)_Value)`

Returns the specified status attribute. If the character or status does not exist, or if the attribute is not of the appropriate type, the query fails.

```c
// ...
AND
NRD_StatusGetGuidString(_Char, _StatusIdx, "StatusSource", _Source)
AND
GetUUID(_Source, _SourceStr)
THEN
DebugBreak("Status source character:");
DebugBreak(_SourceStr);
```

### StatusSet
`call NRD_StatusSetInt((GUIDSTRING)_Character, (INTEGER64)_StatusHandle, (STRING)_Attribute, (INTEGER)_Value)`
`call NRD_StatusSetReal((GUIDSTRING)_Character, (INTEGER64)_StatusHandle, (STRING)_Attribute, (REAL)_Value)`
`call NRD_StatusSetString((GUIDSTRING)_Character, (INTEGER64)_StatusHandle, (STRING)_Attribute, (STRING)_Value)`
`call NRD_StatusSetGuidString((GUIDSTRING)_Character, (INTEGER64)_StatusHandle, (STRING)_Attribute, (GUIDSTRING)_Value)`
`call NRD_StatusSetVector3((GUIDSTRING)_Character, (INTEGER64)_StatusHandle, (STRING)_Attribute, (REAL)_X, (REAL)_Y, (REAL)_Z)`

Updates the specified status attribute.
See the "Status attributes" section below for a list of attributes that can be modified.

**Stability:** Additional real attributes (`Strength`, etc.) will be added when it has been determined that it's safe to update those values during the lifetime of the status.

Example usage:
```c
// Extend status lifetime by 2 turns
// ...
AND
NRD_StatusGetHandle(_Character, "WET", _Handle)
AND
NRD_StatusGetReal(_Character, _Handle, "CurrentLifeTime", _CurrentLifeTime)
AND
RealSum(_CurrentLifeTime, 12.0, _NewLifeTime)
THEN
NRD_StatusSetReal(_Character, _Handle, "CurrentLifeTime", _NewLifeTime);
```

### Status attributes

| Attribute | Type | Access | Description |
|--|--|--|--|
| StatsId | String | Read | Name of the associated stat entry |
| StatusHandle | Integer64 | Read | Handle of this status |
| TargetCI | GuidString | Read | *Unknown; name subject to change* |
| StatusSource | GuidString | Read | Character or item that caused the status |
| Obj2 | GuidString | Read | *Unknown; name subject to change* |
| StartTimer | Real | Read |  |
| LifeTime | Real | Read/Write | Total lifetime of the status, in seconds. -1 if the status does not expire. |
| CurrentLifeTime | Real/Write | Read | Remaining lifetime of the status, in seconds. |
| TurnTimer | Real | Read | Elapsed time in the current turn (0..6) |
| Strength | Real | Read/Write |  |
| StatsMultiplier | Real | Read/Write |  |
| CanEnterChance | Integer | Read | Chance of entering status (between 0 and 100) |
| DamageSourceType | Enum | Read | Cause of status (See `DamageSourceType` enum) |
| KeepAlive | Flag | Read |  |
| IsOnSourceSurface | Flag | Read |  |
| IsFromItem | Flag | Read |  |
| Channeled | Flag | Read |  |
| IsLifeTimeSet | Flag | Read | Does the status have a lifetime or is it infinite? |
| InitiateCombat | Flag | Read |  |
| Influence | Flag | Read |  |
| BringIntoCombat | Flag | Read |  |
| IsHostileAct | Flag | Read |  |
| IsInvulnerable | Flag | Read | The status turns the character invulnerable |
| IsResistingDeath | Flag | Read | The character can't die until the status expires |
| ForceStatus | Flag | Read | The status was forcibly applied (i.e. it bypasses resistance checks) |
| ForceFailStatus | Flag | Read |  |
| RequestDelete | Flag | Read | The status is being deleted (i.e. it's not active anymore) |
| RequestDeleteAtTurnEnd | Flag | Read | The status will be deleted at the end of the current turn |
| Started | Flag | Read |  |

## StatusHeal attributes

| Attribute | Type | Access | Description |
|--|--|--|--|
| EffectTime | Real | Read/Write |  |
| HealAmount | Integer | Read/Write |  |
| HealEffect | Enum | Read/Write |  |
| HealEffectId | String | Read/Write | Default `RS3_FX_GP_ScriptedEvent_Regenerate_01` |
| HealType | Enum | Read/Write | See `StatusHealType` enumeration  |
| AbsorbSurfaceRange | Integer | Read/Write |  |
| TargetDependentHeal | Flag | Read/Write |  |

## StatusHit attributes

| Attribute | Type | Access | Description |
|--|--|--|--|
| SkillId | String | Read/Write | Skill that was used for the attack |
| Equipment | Integer | Read/Write | **TODO** *Meaning not known.* |
| DeathType | Enum | Read/Write | A value from the `Death Type` enumeration |
| DamageType | Enum | Read/Write | A value from the `Damage Type` enumeration |
| AttackDirection | Enum | Read/Write | See `AttackDirection` enumeration. *Purpose not known.* |
| ArmorAbsorption | Integer | Read/Write |  |
| LifeSteal | Integer | Read/Write |  |
| HitWithWeapon | Integer | Read/Write |  |
| HitReason | Integer | Read/Write |  |
| Hit | Flag | Read/Write | The attack hit |
| Blocked | Flag | Read/Write | The attack was blocked |
| Dodged | Flag | Read/Write | The attack was dodged |
| Missed | Flag | Read/Write | The attack missed |
| CriticalHit | Flag | Read/Write |  |
| AlwaysBackstab | Flag | Read/Write | Equivalent to the `AlwaysBackstab` skill property |
| FromSetHP | Flag | Read/Write | Indicates that the hit was called from `CharacterSetHitpointsPercentage` (or similar) |
| DontCreateBloodSurface | Flag | Read/Write | Avoids creating a blood surface when the character is hit |
| Reflection | Flag | Read/Write |  |
| NoDamageOnOwner | Flag | Read/Write |  |
| FromShacklesOfPain | Flag | Read/Write |  |
| DamagedMagicArmor | Flag | Read/Write | Indicates that the hit damaged magic armor |
| DamagedPhysicalArmor | Flag | Read/Write | Indicates that the hit damaged physical armor |
| DamagedVitality | Flag | Read/Write | Indicates that the hit damaged the characters vitality |
| PropagatedFromOwner | Flag | Read/Write |  |
| ProcWindWalker | Flag | Read/Write | Hit should proc the Wind Walker talent |
| ImpactPosition | Vector3 | Read/Write |  |
| ImpactOrigin | Vector3 | Read/Write |  |
| ImpactDirection | Vector3 | Read/Write |  |

# Hit functions

`call NRD_HitPrepare((GUIDSTRING)_Target, (GUIDSTRING)_Source)`
`call NRD_HitExecute()`
`call NRD_HitSetInt((STRING)_Property, (INTEGER)_Value)`
`call NRD_HitSetString((STRING)_Property, (STRING)_Value)`
`call NRD_HitSetVector3((STRING)_Property, (REAL)_X, (REAL)_Y, (REAL)_Z)`
`call NRD_HitSetFlag((STRING)_Flag)`
`call NRD_HitAddDamage((INTEGER)_DamageType, (INTEGER)_Amount)`

The Hit API is an extension of `ApplyDamage()` with many additional features. 
**Usage steps:**
 - Call `NRD_HitPrepare()`. This will clear the variables set by previous hit calls, and prepares a new hit call.
 - Set hit parameters by calling the `NRD_HitSet...` functions
 - Add one or more damage types by calling `NRD_HitAddDamage()`
 - Apply the hit using `NRD_HitExecute()`

In addition to the parameters listed in [StatusHit attributes](#statushit-attributes) the following parameters can be set when launching a hit:

| Attribute | Type | Description |
|--|--|--|
| CallCharacterHit | Flag | **TODO this is complex!** |
| HitType | Integer | **TODO** *Used in many places, meaning not known. Possibly a bitmask. Values 0, 1, 2, 4 and 5 have special significance* |
| RollForDamage | Integer | Determines if the hit is guaranteed. 0 = An RNG roll determines whether the attack hits or is dodged/missed/blocked; the appropriate flag (`Hit`, `Dodged`, `Missed`, `Blocked`) is set automatically. 1 = No RNG roll is performed and the attack always hits; flag `Hit` is set automatically. |
| RollForDamage | Integer | Determines whether the hit is guaranteed. 0 = An RNG roll determines whether the attack hits or is dodged/missed/blocked; the appropriate flag (`Hit`, `Dodged`, `Missed`, `Blocked`) is set automatically. 1 = No RNG roll is performed and the attack always hits; flag `Hit` is set automatically. |
| CriticalRoll | Integer | Determines the outcome of the critical hit roll. 0 = An RNG roll determines whether the attack is a critical hit; flag `CriticalHit` is set depending on the result. 1 = The hit is always a critical hit; flag `CriticalHit` is set automatically. 2 = The hit is not a critical hit. |
| ProcWindWalker | Flag |  |
| HighGround | Integer | High ground bonus indicator. 0 = High ground test not performed. 1 = Attacker is on high ground. 2 = Attacker is on even ground. 3 = Attacker is on low ground. |

**Notes:**
 - Make sure that both `NRD_HitPrepare` and `NRD_HitExecute` are called in the same rule/proc and that there are no calls between the two that might trigger other events, to ensure that other scripts can't interfere with the hit.

**Example usage (normal hit):**
```c
NRD_HitPrepare(CHARACTERGUID_Sandbox_Arena_Shae_734e8ad4-c1ea-4c69-b5ad-310d28bf9462, CHARACTERGUID_Sandbox_Market_Ernest_Herringway_da8d55ba-0855-4147-b706-46bbc67ec8b6);
NRD_HitAddDamage(8, 50);
NRD_HitSetInt("CallCharacterHit", 1);
NRD_HitSetInt("CriticalRoll", 1);
NRD_HitExecute();
```

**Example usage (manually controlled hit):**
```c
NRD_HitPrepare(CHARACTERGUID_Sandbox_Arena_Shae_734e8ad4-c1ea-4c69-b5ad-310d28bf9462, CHARACTERGUID_Sandbox_Market_Ernest_Herringway_da8d55ba-0855-4147-b706-46bbc67ec8b6);
NRD_HitAddDamage(2, 50);
NRD_HitSetFlag("Hit");
NRD_HitSetFlag("DamagedVitality");
NRD_HitExecute();
```

### OnHit
`event NRD_OnHit((GUIDSTRING)_Target, (GUIDSTRING)_Instigator, (INTEGER)_Damage, (INTEGER64)_StatusHandle)`

Thrown before a character is hit. Status attributes can be queried using the `NRD_StatusGet[...]`  functions. For a list of attributes, see [Status attributes](#status-attributes) and [StatusHit attributes](#statushit-attributes).

**Stability:**
 - Since the event is thrown before the hit occurs, it is possible to modify or cancel the hit effect. API-s for this will be available in a future version.

### OnHeal
`event NRD_OnHeal((GUIDSTRING)_Target, (GUIDSTRING)_Instigator, (INTEGER)_Amount, (INTEGER64)_StatusHandle)`

Thrown before a character is healed. Status attributes can be queried using the `NRD_StatusGet[...]`  functions. For a list of attributes, see [Status attributes](#status-attributes) and [StatusHeal attributes](#statusheal-attributes).

**Stability:**
 - Since the event is thrown before heal occurs, it is possible to modify or cancel the heal effect. API-s for this will be available in a future version.


# Projectile functions

`call NRD_ProjectilePrepareLaunch()`
`call NRD_ProjectileLaunch()`
`call NRD_ProjectileSetInt((STRING)_Property, (INTEGER)_Value)`
`call NRD_ProjectileSetString((STRING)_Property, (STRING)_Value)`
`call NRD_ProjectileSetVector3((STRING)_Property, (REAL)_X, (REAL)_Y, (REAL)_Z)`
`call NRD_ProjectileSetGuidString((STRING)_Property, (GUIDSTRING)_Value)`

The projectile API is a set of functions for casting Projectile/ProjectileStrike skills. It is an extension of `CreateProjectileStrikeAt` and `CreateExplosion`.

**Usage:**
A cast must be prepared by calling `NRD_BeginProjectile()`. Projectile parameters must be set by calling the `NRD_ProjectileSetString/GuidString/Int/Vector3` functions. When all parameters are set, the skill is cast by calling `NRD_EndProjectile()`.

**Stability:**
Function signatures are final. Projectile parameters that are not documented are subject to change. An additional `NRD_ProjectileAddDamage` call may be introduced later on.

**Projectile parameters:**
| Parameter | Type | Description |
|--|--|--|--|
| SkillId | String | Skill to cast. Must be a Projectile or ProjectileStrike skill |
| FS2 | String | *Unknown; name subject to change* |
| CasterLevel | Integer |  |
| StatusClearChance | Integer |  |
| IsTrap | Flag |  |
| UnknownFlag1 | Flag | *Unknown; name subject to change* |
| HasCaster | Flag |  |
| IsStealthed| Flag |  |
| UnknownFlag2 | Flag |  |
| SourcePosition | Vector3 | Launch projectile from the specified position |
| SourcePosition | GuidString | Launch projectile from the position of the specified character/item |
| TargetPosition | Vector3 | Launch projectile towards the specified position|
| TargetPosition | GuidString | Launch projectile towards the position of the specified character/item |
| Source | GuidString | Caster character |
| Target | GuidString | Target character |
| Target2 | GuidString | *Unknown; name subject to change* |

**Notes:**
 - For a successful cast, at least `SkillId`, `SourcePosition` and `TargetPosition` (either Vector3 or GuidString) must be set.
 - Calling `ProjectileSetGuidString` with `SourcePosition` or `TargetPosition` is a shortcut for calling `GetPosition()` and `NRD_ProjectileSetVector3()` for the specified character/item.
 - The character/item specified when setting `SourcePosition` is not considered to be the caster of the skill, and the cast will trigger no adverse reaction towards the caster. To set the caster character, set the `Source` property as well.
 - Make sure that you call `NRD_ProjectilePrepareLaunch` and `NRD_ProjectileLaunch` in the same rule/proc and that there are no calls between the two that might trigger other events, to ensure that other scripts can't interfere with the projectile.

**Example:**
```c
NRD_ProjectilePrepareLaunch();
NRD_ProjectileSetString("SkillId", "Projectile_Grenade_PoisonFlask");
NRD_ProjectileSetInt("CasterLevel", 3);
NRD_ProjectileSetGuidString("SourcePosition", Sandbox_Market_Ernest_Herringway_da8d55ba-0855-4147-b706-46bbc67ec8b6);
NRD_ProjectileSetGuidString("TargetPosition", Sandbox_Market_Ernest_Herringway_da8d55ba-0855-4147-b706-46bbc67ec8b6);
NRD_ProjectileLaunch();
```

# Skill functions

### SkillSetCooldown
`call NRD_SkillSetCooldown((GUIDSTRING)_Character, (STRING)_SkillId, (REAL)_Cooldown)`

Set the current cooldown timer of the skill (in seconds).
Doesn't work on skills that can only be used once per combat.

### SkillGetCooldown
`query NRD_SkillGetCooldown([in](GUIDSTRING)_Character, [in](STRING)_SkillId, [out](REAL)_Cooldown)`

Returns the current cooldown timer of the skill (in seconds).
For skills that can only be used once per combat -1.0 is returned.

# Skillbar functions

**Notes:**
 - The skill bar functions can only be used on player characters
 - The skill bar slot (`_Slot`) must be an integer between 0 and 144.

### SkillBarGetItem
`query NRD_SkillBarGetItem([in](GUIDSTRING)_Character, [in](INTEGER)_Slot, [out](GUIDSTRING)_Item)`

Retrieves the item in skill bar slot `_Slot`. If the character is not a player, the skill bar slot is empty, or if the skill bar slot is occupied by a skill, the query fails.

### SkillBarGetSkill
`query NRD_SkillBarGetSkill([in](GUIDSTRING)_Character, [in](INTEGER)_Slot, [out](STRING)_Skill)`

Retrieves the skill in skill bar slot `_Slot`. If the character is not a player, the skill bar slot is empty, or if the skill bar slot is occupied by an item, the query fails.

### SkillBarFindSkill
`query NRD_SkillBarFindSkill([in](GUIDSTRING)_Character, [in](STRING)_Skill, [out](INTEGER)_Slot)`

Checks whether the player has the skill `_Skill` on the skill bar, and returns the slot number of its first occurrence in `_Slot`. If the character is not a player or the skill is not on the skill bar, the query fails.

### SkillBarFindItem
`query NRD_SkillBarFindItem([in](GUIDSTRING)_Character, [in](GUIDSTRING)_Item, [out](INTEGER)_Slot)`

Checks whether the player has the item `_Item` on the skill bar, and returns the slot number of its first occurrence in `_Slot`. If the character is not a player or the item is not on the skill bar, the query fails.

### SkillBarSetSkill
`call NRD_SkillBarSetSkill((GUIDSTRING)_Character, (INTEGER)_Slot, (STRING)_SkillId)`

Removes any item or skill that's currently in the skill bar slot and replaces it with the specified skill.

**Notes:**
 - It is possible to pin any skill to the skill bar, not just ones that the character currently has.

### SkillBarSetItem
`call NRD_SkillBarSetItem((GUIDSTRING)_Character, (INTEGER)_Slot, (GUIDSTRING)_Item)`

Removes any item or skill that's currently in the skill bar slot and replaces it with the specified item.

### SkillBarClear
`call NRD_SkillBarClear((GUIDSTRING)_Character, (INTEGER)_Slot)`

Removes any item or skill that's currently in the specified skill bar slot.


# Game Action functions

These functions create game actions (Rain/Storm/etc.), but bypass the skill casting system entirely (doesn't consume AP, doesn't play skill use animations, doesn't reset skill cooldowns).
Each function needs a skill stats id of the same type (Rain skill id for `NRD_CreateRain`, etc.)

**Stability:**
 - The following additional options may be added:
   - Global: `ActivateTimer`
   - Tornado: `TurnTimer`, `IsFromItem`, `Finished`
   - Storm: `TurnTimer`, `LifeTime`, `IsFromItem`, `Finished`
   - Rain: `TurnTimer`, `LifeTime`, `Duration`, `IsFromItem`, `Finished`
   - Wall: `TurnTimer`, `LifeTime`, `IsFromItem`, `Finished`
   - Dome: `LifeTime`, `Finished`

### CreateRain
`query NRD_CreateRain([in](GUIDSTRING)_OwnerCharacter, [in](STRING)_SkillId, [in](REAL)_X, [in](REAL)_Y, [in](REAL)_Z, [out](INTEGER64)_GameObjectHandle)`

### CreateStorm
`query NRD_CreateStorm([in](GUIDSTRING)_OwnerCharacter, [in](STRING)_SkillId, [in](REAL)_X, [in](REAL)_Y, [in](REAL)_Z, [out](INTEGER64)_GameObjectHandle)`

### CreateWall
`query NRD_CreateWall([in](GUIDSTRING)_OwnerCharacter, [in](STRING)_SkillId, [in](REAL)_SourceX, [in](REAL)_SourceY, [in](REAL)_SourceZ, [in](REAL)_TargetX, [in](REAL)_TargetY, [in](REAL)_TargetZ, [out](INTEGER64)_GameObjectHandle)`

### CreateTornado
`query NRD_CreateTornado([in](GUIDSTRING)_OwnerCharacter, [in](STRING)_SkillId, [in](REAL)_PositionX, [in](REAL)_PositionY, [in](REAL)_PositionZ, [in](REAL)_TargetX, [in](REAL)_TargetY, [in](REAL)_TargetZ, [out](INTEGER64)_GameObjectHandle)`

### CreateDome
`query NRD_CreateDome([in](GUIDSTRING)_OwnerCharacter, [in](STRING)_SkillId, [in](REAL)_X, [in](REAL)_Y, [in](REAL)_Z, [out](INTEGER64)_GameObjectHandle)`

### GameObjectMove
`query NRD_CreateGameObjectMove([in](GUIDSTRING)_TargetCharacter, [in](REAL)_X, [in](REAL)_Y, [in](REAL)_Z, [in](STRING)_BeamEffectName, [in](GUIDSTRING)_CasterCharacter, [out](INTEGER64)_GameObjectHandle)`

### GameActionDestroy
`call NRD_GameActionDestroy((INTEGER64)_GameActionHandle)`

Destroys the specified game action. `_GameActionHandle` is the handle returned from one of the game action create functions (`CreateWall`, `CreateDome`, etc).

**Notes:**
 - The game action is not destroyed immediately, but on the next tick. This means that the game action might survive the current turn (up to 6 seconds) and will exit afterwards. This behavior might change in the future if a safer way is found to destroy game actions.

### GameActionGetLifeTime
`query NRD_GameActionGetLifeTime([in](INTEGER64)_GameActionHandle, [out](REAL)_LifeTime)`

Returns the total lifetime of the specified game action. 

**Notes:**
 - Only `Rain`, `Storm`, `Wall` and `Dome` actions are supported.


### GameActionSetLifeTime
`call NRD_GameActionSetLifeTime((INTEGER64)_GameActionHandle, (REAL)_LifeTime)`

Updates the total lifetime of the specified game action.

**Notes:**
 - Only `Rain`, `Storm` and `Dome` actions are supported; the others cannot be controlled directly via the lifetime variable.


### Summon
`query NRD_Summon([in](GUIDSTRING)_OwnerCharacter, [in](GUIDSTRING)_Template, [in](REAL)_X, [in](REAL)_Y, [in](REAL)_Z, [in](REAL)_Lifetime, [in](INTEGER)_Level, [in](INTEGER)_IsTotem, [in](INTEGER)_MapToAiGrid, [out](GUIDSTRING)_Summon)`

** TODO Documentation **

# Item functions

### ItemSetIdentified
`call NRD_ItemSetIdentified((GUIDSTRING)_Item, (INTEGER)_IsIdentified)`

Sets whether the item is identified or not.
Marking common items (that cannot be identified) as unidentified has no effect.

### ItemGetStatsId
`query NRD_ItemGetStatsId([in](GUIDSTRING)_Item, [out](STRING)_StatsId)`

Return the stats entry ID of the specified item.

### ItemGetGenerationParams
`query NRD_ItemGetGenerationParams([in](GUIDSTRING)_Item, [out](STRING)_Base, [out](STRING)_ItemType, [out](INTEGER)_Level)`

Return the stats generation parameters of the specified item.

### ItemHasDeltaModifier
`query NRD_ItemHasDeltaModifier([in](GUIDSTRING)_Item, [in](STRING)_DeltaMod, [out](INTEGER)_Count)`

Return the number of DeltaMods on the item with the specified boost name. Unlike vanilla `ItemHasDeltaModifier`, this also takes into account the boosts added by item generation.


## Permanent Boosts
Permanent Boosts are stat bonuses or stat reductions that are applied to an item. They are permanent, i.e. are stored in the savegame.

| Attribute | Type | Item Type |
|--|--|--|
| Durability | Integer | Any |
| DurabilityDegradeSpeed | Integer | Any |
| StrengthBoost | Integer | Any |
| FinesseBoost | Integer | Any |
| IntelligenceBoost | Integer | Any |
| ConstitutionBoost | Integer | Any |
| Memory  | Integer | Any |
| WitsBoost | Integer | Any |
| SightBoost | Integer | Any |
| HearingBoost | Integer | Any |
| VitalityBoost | Integer | Any |
| SourcePointsBoost | Integer | Any |
| MaxAP | Integer | Any |
| StartAP | Integer | Any |
| APRecovery | Integer | Any |
| AccuracyBoost | Integer | Any |
| DodgeBoost | Integer | Any |
| AccuracyBoost | Integer | Any |
| LifeSteal | Integer | Any |
| CriticalChance | Integer | Any |
| ChanceToHitBoost | Integer | Any |
| MovementSpeedBoost | Integer | Any |
| RuneSlots  | Integer | Any |
| RuneSlots_V1 | Integer | Any |
| FireResistance | Integer | Any |
| AirResistance | Integer | Any |
| WaterResistance | Integer | Any |
| EarthResistance | Integer | Any |
| PoisonResistance | Integer | Any |
| TenebriumResistance | Integer | Any |
| PiercingResistance | Integer | Any |
| CorrosiveResistance | Integer | Any |
| PhysicalResistance | Integer | Any |
| MagicResistance | Integer | Any |
| CustomResistance | Integer | Any |
| Movement | Integer | Any |
| Initiative | Integer | Any |
| Willpower | Integer | Any |
| Bodybuilding | Integer | Any |
| MaxSummons | Integer | Any |
| Value | Integer | Any |
| Weight | Integer | Any |
| DamageType | Integer | Weapon |
| MinDamage | Integer | Weapon |
| MaxDamage | Integer | Weapon |
| DamageBoost | Integer | Weapon |
| DamageFromBase | Integer | Weapon |
| CriticalDamage | Integer | Weapon |
| WeaponRange | Real | Weapon |
| CleaveAngle | Integer | Weapon |
| CleavePercentage | Real | Weapon |
| AttackAPCost | Integer | Weapon |
| ArmorValue | Integer | Armor/Shield |
| ArmorBoost | Integer | Armor/Shield |
| MagicArmorValue | Integer | Armor/Shield |
| MagicArmorBoost | Integer | Armor/Shield |
| Blocking | Integer | Shield |

**Limitations:**
Permanent boosts don't show up immediately because of how client-server communication works in the game. To ensure that boosts are visible on the client:
 - For new items, the item and the permanent boost should be created in the same tick (e.g. an `ItemSetPermanentBoost` call immediately after `CreateItemTemplateAtPosition`)
 - For existing items, the item should be cloned after calling `ItemSetPermanentBoost`, and the original item should be replaced by the clone. (The permanent boosts are visible on the clone, but not on the original item.)
 - A save/reload also makes boosts visible


### ItemGetPermanentBoost
`query NRD_ItemGetPermanentBoostInt([in](GUIDSTRING)_Item, [in](STRING)_Stat, [out](INTEGER)_Value)`
`query NRD_ItemGetPermanentBoostReal([in](GUIDSTRING)_Item, [in](STRING)_Stat, [out](REAL)_Value)`

Returns the permanent boost value applied to the specified item. `_Stat` must be one of the values listed above.


### ItemSetPermanentBoost
`query NRD_ItemGetPermanentBoostInt([in](GUIDSTRING)_Item, [in](STRING)_Stat, [out](INTEGER)_Value)`
`query NRD_ItemGetPermanentBoostReal([in](GUIDSTRING)_Item, [in](STRING)_Stat, [out](REAL)_Value)`

Updates the permanent boost value of `_Stat` to the specified value . `_Stat` must be one of the values listed above. Both positive and negative boost values are supported.


## Cloning items

`call NRD_ItemCloneBegin((GUIDSTRING)_Item)`
`call NRD_ItemCloneSetInt((STRING)_Property, (INTEGER)_Value)`
`call NRD_ItemCloneSetString((STRING)_Property, (STRING)_Value)`
`query NRD_ItemClone([out](GUIDSTRING)_NewItem)`

**Usage:**
The clone API creates a copy of a specific item.
To start cloning an item, call `NRD_ItemCloneBegin()`. Additional modifications can be applied to the newly created item by calling `NRD_ItemCloneSetXyz(...)`. After the parameter modifications were performed, the clone operation is finished by calling `NRD_ItemClone()`.

**Clone parameters (passed to `NRD_ItemCloneSetXyz`):**
| Attribute | Type | Description |
|--|--|--|--|
| RootTemplate | GuidString | Root template of the new item |
| OriginalRootTemplate | GuidString | Original root template (to be used for items that are already transformed during cloning) |
| Amount | Integer | Number of items |
| Slot | Integer | Original inventory slot of item |
| GoldValueOverwrite | Integer | Overrides the gold value of the item |
| WeightValueOverwrite | Integer | Overrides the weight of the item |
| DamageTypeOverwrite | Integer | Overrides the damage type of the item |
| ItemType | String | Item rarity (eg. `Uncommon`) |
| GenerationStatsId | String | Stats used to generate the item (eg. `WPN_Shield`) |
| GenerationItemType | String | Item rarity used to generate the item (eg. `Uncommon`) |
| GenerationRandom | Integer | Random seed for item generation |
| GenerationLevel | Integer | Level of generated item |
| StatsEntryName | String | Stats ID of the item (eg. `WPN_Shield`) |
| HasModifiedSkills | Flag | Indicates that the skills of the item were overridden |
| Skills | String | Item skills |


# Miscellaneous functions

### DebugLog 
`call NRD_DebugLog((STRING)_Message)`

Functionally equivalent to `DebugBreak()`, except that the `_Message` argument accepts any type, not only strings. To use non-string arguments, cast the variable to the appropriate type.

Example usage:  
```c
IF
StoryEvent((ITEMGUID)_Item, "TEST")
THEN
NRD_DebugLog((STRING)_Item);
```


### ForLoop
`call NRD_ForLoop((STRING)_Event, (INTEGER)_Count)`
`call NRD_ForLoop((GUIDSTRING)_Object, (STRING)_Event, (INTEGER)_Count)`
`event NRD_Loop((STRING)_Event, (INTEGER)_Num)`
`event NRD_Loop((GUIDSTRING)_Object, (STRING)_Event, (INTEGER)_Num)`

Counts from 0 up to `_Count - 1` and throws loop event `_Event` for each value. Unlike regular events, `NRD_Loop` are not queued and are thrown immediately (i.e. during the `NRD_ForLoop` call), so there is no need for an additional cleanup/finalizer event.

Example usage:  
```c
// ...
NRD_ForLoop("MyMod_SomeLoopEvent", 10);

IF
NRD_Loop("MyMod_SomeLoopEvent", _Int)
THEN
NRD_DebugLog((STRING)_Int);
```
  
Example usage with GUIDs:
```c
// ...
NRD_ForLoop(CHARACTERGUID_S_GLO_CharacterCreationDummy_001_da072fe7-fdd5-42ae-9139-8bd4b9fca406, "MyMod_SomeLoopEvent", 10);
  
IF
NRD_Loop((CHARACTERGUID)_Char, "MyMod_SomeLoopEvent", _Int)
THEN
NRD_DebugLog((STRING)_Char);
NRD_DebugLog((STRING)_Int);
```
  
  

# Math functions

### Sin
`query NRD_Sin([in](REAL)_In, [out](REAL)_Out)`

Computes the sine of the argument `_In` (measured in radians).
  

### Cos
`query NRD_Cos([in](REAL)_In, [out](REAL)_Out)`

Computes the cosine of the argument `_In` (measured in radians).
  

### Tan
`query NRD_Tan([in](REAL)_In, [out](REAL)_Out)`

Computes the tangent of the argument `_In` (measured in radians).
  

### Round
`query NRD_Round([in](REAL)_In, [out](REAL)_Out)`

Computes the nearest integer value to the argument `_In`, rounding halfway cases away from zero.
  

### Ceil
`query NRD_Ceil([in](REAL)_In, [out](REAL)_Out)`

Computes the smallest integer value not less than the argument.


### Floor
`query NRD_Floor([in](REAL)_In, [out](REAL)_Out)`

Computes the largest integer value not greater than the argument.
  

### Abs
`query NRD_Abs([in](REAL)_In, [out](REAL)_Out)`

Computes the absolute value of the argument.
  

### Pow
`query NRD_Pow([in](REAL)_Base, [in](INTEGER)_Exp, [out](REAL)_Out)`

Computes the value of `_Base` raised to the power `_Exp`.
  

### Sqrt
`query NRD_Sqrt([in](REAL)_In, [out](REAL)_Out)`

Computes the square root of `_In`.
  

### Exp
`query NRD_Exp([in](REAL)_In, [out](REAL)_Out)`

Computes `e` (Euler's number, 2.7182818...) raised to the given power `_In`.

### Log
`query NRD_Log([in](REAL)_In, [out](REAL)_Out)`

Computes the natural (base `e`) logarithm of `_In`.


### Factorial
`query NRD_Factorial([in](INTEGER)_In, [out](INTEGER)_Out)`

Computes the factorial of the value `_In`.

### RandomReal
`query NRD_RandomReal([in](REAL)_Min, [in](REAL)_Max, [out](REAL)_Result)`

Returns uniformly distributed random numbers in the range [`_Min` ... `_Max`].

  
# String functions

### StringCompare
`query NRD_StringCompare([in](STRING)_A, [in](STRING)_B, [out](INTEGER)_Result)`

Compare strings `A` and `B` using lexicographic ordering.
**Value of `Result`:**
 - -1, if `A` comes before `B`
 - 0, if `A` is equal to `B`
 - 1, if `A` comes after `B`

### StringLength
`query NRD_StringLength([in](STRING)_String, [out](INTEGER)_Length)`

Computes the length of the string `_String`, and returns it in `_Length`.

### StringToInt
`query NRD_StringToInt([in](STRING)_String, [out](INTEGER)_Result)`

Attempts to convert `_String` to an integer value. If the conversion succeeds (i.e. the string is a valid integer), the integer value is returned in `_Result`. If `_String` is not a valid integer, the query fails.

For detailed rules [check the reference](https://en.cppreference.com/w/cpp/string/basic_string/stol)


### StringToReal
`query NRD_StringToReal([in](STRING)_String, [out](REAL)_Result)`

Attempts to convert `_String` to a real value. If the conversion succeeds (i.e. the string is a valid real), the real value is returned in `_Result`. If `_String` is not a valid real value, the query fails.

For detailed rules see [check the reference](https://en.cppreference.com/w/cpp/string/basic_string/stof)


### RealToString
`query NRD_RealToString([in](REAL)_Real, [out](STRING)_Result)`

Converts `_Real` to a string value.


# Enumerations

### DamageSourceType
| Value | Label |
|--|--|
| 0 | None |
| 1 | SurfaceMove |
| 2 | SurfaceCreate |
| 3 | SurfaceStatus |
| 4 | StatusEnter |
| 5 | StatusTick |
| 6 | Attack |
| 7 | Offhand |
| 8 | GM |


### DeathType
| Value | Label |
|--|--|
| 0 | None |
| 1 | Physical |
| 2 | Piercing |
| 3 | Arrow |
| 4 | DoT |
| 5 | Incinerate |
| 6 | Acid |
| 7 | Electrocution |
| 8 | FrozenShatter |
| 9 | PetrifiedShatter |
| 10 | Explode |
| 11 | Surrender |
| 12 | Hang |
| 13 | KnockedDown |
| 14 | Lifetime |
| 15 | Sulfur |
| 16 | Sentinel (default value) |


### AttackDirection
| Value | Label |
|--|--|
| 0 | FrontToBack_Upper |
| 1 | FrontToBack_Lower |
| 2 | LeftToRight_Upper |
| 3 | LeftToRight_Lower |
| 4 | RightToLeft_Upper |
| 5 | RightToLeft_Lower |
| 6 | UpToDown |
| 7 | DownToUp |