#include "stdafx.h"
#include "Plugin.h"
#include "IExamInterface.h"
#include "SteeringBehaviours.h"

#include <cmath> 
//Called only once, during initialization
void Plugin::Initialize(IBaseInterface* pInterface, PluginInfo& info)
{
	//Retrieving the interface
	//This interface gives you access to certain actions the AI_Framework can perform for you
	m_pInterface = static_cast<IExamInterface*>(pInterface);

	//Bit information about the plugin
	//Please fill this in!!
	info.BotName = "Spinny";
	info.Student_FirstName = "Stef";
	info.Student_LastName = "Janssens";
	info.Student_Class = "2DAE05";
}

//Called only once
void Plugin::DllInit()
{
	//Can be used to figure out the source of a Memory Leak
	//Possible undefined behavior, you'll have to trace the source manually 
	//if you can't get the origin through _CrtSetBreakAlloc(0) [See CallStack]
	//_CrtSetBreakAlloc(0);

	//Called when the plugin is loaded

	m_pSeekBehaviour = new Seek();

	//Condition lambda's
#pragma region conditions
	auto hasHouseInFovAndNotOldHouse = [&]()->bool {
		auto vHousesInFOV = GetHousesInFOV();
		return vHousesInFOV.size() > ZERO && vHousesInFOV[ZEROINT].Center != m_centerHouseLastVisited && !m_isCooldown;
	};

	auto hasItemInRange = [&]()->bool {
		auto vEntitiesInFOV = GetEntitiesInFOV();
		for (auto entity : vEntitiesInFOV)
		{
			if (entity.Type == eEntityType::ITEM)
			{
				//Gather Item Info
				auto dir = entity.Location - m_agent.Position;
				if (Distance(m_agent.Position, entity.Location) < m_agent.GrabRange - GRABRANGEDIFF && dir.Magnitude() < m_agent.GrabRange - GRABRANGEDIFF) {
					m_itemInRange = entity;
					return true;
				}
			}
		}
		return false;
	};

	auto hasPickUpItem = [&]()->bool {
		return m_pickedUp;
	};



	auto hasItemOutOfRange = [&]()->bool {
		auto vEntitiesInFOV = GetEntitiesInFOV();
		bool outOfRange = false;
		for (auto entity : vEntitiesInFOV)
		{
			if (entity.Type == eEntityType::ITEM)
			{
				//Gather Item Info
				auto dir = entity.Location - m_agent.Position;
				if (Distance(m_agent.Position, entity.Location) > m_agent.GrabRange - GRABRANGEDIFF && dir.Magnitude() > m_agent.GrabRange - GRABRANGEDIFF) {
					m_itemOutRange = entity;
					return true;
				}
			}
		}
		return false;
	};

	auto beenInHouseFor360Degree = [&]()->bool {
		return m_hasTurned;
	};

	auto hasEnemyInSight = [&](float& dt)->bool {
		auto vEntitiesInFOV = GetEntitiesInFOV(); //uses m_pInterface->Fov_GetEntityByIndex(...)

		for (auto entity : vEntitiesInFOV)
		{
			if (entity.Type == eEntityType::ENEMY)
			{
				return true;
			}
		}
		if (m_agent.Bitten) return true;
		return false;
	};

	auto isCloseToCenter = [&]()->bool {
		if (Distance(m_agent.Position, m_centerHouseToVisit) < 1.5f) {
			return true;
		}
		return false;
	};

	//Actions if conditions are met
	auto setCenterHouse = [&]() {
		auto vHousesInFOV = GetHousesInFOV();
		m_centerHouseToVisit = vHousesInFOV[0].Center;
	};

	auto hasItemUsed = [&]() {
		return m_itemUsed;
	};

	auto shouldUseGun = [&]()->bool {
		if (m_hasGun && !m_itemUsed) {
			auto vEntitiesInFOV = GetEntitiesInFOV();
			for (auto entity : vEntitiesInFOV)
			{
				if (entity.Type == eEntityType::ENEMY)
				{
					Vector2 orienVec = Elite::OrientationToVector(m_agent.Orientation);
					orienVec.Normalize();
					ItemInfo iInfo = {};
					m_pInterface->Inventory_GetItem(GUN, iInfo);
					float range = m_pInterface->Item_GetMetadata(iInfo, RANGE);

					float length = 0;
					EnemyInfo enemy;
					m_pInterface->Enemy_GetInfo(entity, enemy);
					while (length < range) {
						Vector2 otherEnd = orienVec * length + m_agent.Position;
						if (otherEnd.x < enemy.Location.x + (enemy.Size / DIVIDE2)- SHOOTRANGEDIFF && otherEnd.x > enemy.Location.x - (enemy.Size / DIVIDE2)+ SHOOTRANGEDIFF
							&& otherEnd.y < enemy.Location.y + (enemy.Size / DIVIDE2)- SHOOTRANGEDIFF && otherEnd.y > enemy.Location.y - (enemy.Size / DIVIDE2)+ SHOOTRANGEDIFF) {
							return true;
						}

						length += enemy.Size;
					}
				}
			}
		}
		return false;
	};

	auto shouldUseMedkit = [&]()->bool {
		if (m_hasMedkit && !m_itemUsed) {
			ItemInfo invenItem = {};
			m_pInterface->Inventory_GetItem(MEDKIT, invenItem);
			int currentHealth = m_pInterface->Item_GetMetadata(invenItem, HEALTH); //INT
			if (MAXHEALTH - currentHealth > m_agent.Health) {
				return true;
			}
		}
		return false;
	};

	auto shouldUseFood = [&]()->bool {
		if (m_hasFood && !m_itemUsed) {
			ItemInfo invenItem = {};
			m_pInterface->Inventory_GetItem(FOOD, invenItem);
			int currentEnergy = m_pInterface->Item_GetMetadata(invenItem, ENERGY); //INT
			if (MAXENERGY - currentEnergy > m_agent.Energy) {
				return true;
			}
		}
		return false;
	};

	auto shouldUseFood2 = [&]()->bool {
		if (m_hasFood2) {
			ItemInfo invenItem = {};
			m_pInterface->Inventory_GetItem(FOOD2, invenItem);
			int currentEnergy = m_pInterface->Item_GetMetadata(invenItem, ENERGY); //INT
			if (MAXENERGY - currentEnergy > m_agent.Energy) {
				return true;
			}
		}
		return false;
	};
#pragma endregion

	//Actions
#pragma region actions
	auto SetTargetHouse = [&]() {
		//cout << "house state" << endl;
		m_pSeekBehaviour->SetAngle(ANGLESPEED);
		m_pSeekBehaviour->SetAutoRotate(false);
		m_pSeekBehaviour->SetRunning(true);
		m_pointToGo = m_pInterface->NavMesh_GetClosestPathPoint(m_centerHouseToVisit);
		m_pSeekBehaviour->SetTarget(&m_pointToGo);
	};

	auto SetTargetCheckPoint = [&]() {
		//cout << " checkpoint state" << endl;
		m_pSeekBehaviour->SetAngle(ANGLESPEED);
		m_pSeekBehaviour->SetAutoRotate(false);
		m_pSeekBehaviour->SetRunning(false);
		auto checkpointLocation = m_pInterface->World_GetCheckpointLocation();
		m_pointToGo = m_pInterface->NavMesh_GetClosestPathPoint(checkpointLocation);
		m_pSeekBehaviour->SetTarget(&m_pointToGo);
	};

	auto setCenterHouseLastVisited = [&]() {
		m_centerHouseLastVisited = m_centerHouseToVisit;
	};

	auto resetTurning = [&]() {
		m_hasTurned = false;
		m_turned = ZERO;
		m_isCooldown = true;
	};

	auto SetTargetItem = [&]() {
		//cout << "go to item state" << endl;
		m_pSeekBehaviour->SetAutoRotate(true);
		m_pSeekBehaviour->SetRunning(false);
		m_pointToGo = m_pInterface->NavMesh_GetClosestPathPoint(m_itemOutRange.Location);
		m_pSeekBehaviour->SetTarget(&m_pointToGo);
	};

	auto Move = [&]() {
		if (m_agent.Stamina > RUNTO) {
			m_pSeekBehaviour->SetRunning(true);
		}
		if (m_agent.Bitten) {
			m_pSeekBehaviour->SetRunning(true);
		}
		auto vEntitiesInFOV = GetEntitiesInFOV();
		for (auto entity : vEntitiesInFOV)
		{
			if (entity.Type == eEntityType::ENEMY)
			{
				m_pSeekBehaviour->SetRunning(true);
			}
		}

		m_steering = m_pSeekBehaviour->CalculateSteering(m_deltaTime, &m_agent);
		//m_steering.LinearVelocity = m_pointToGo - m_agent.Position; //Desired Velocity
		//m_steering.LinearVelocity.Normalize(); //Normalize Desired Velocity
		//m_steering.LinearVelocity *= m_agent.MaxLinearSpeed;
	};

	auto Turning = [&](float& dt) {
		//cout << "turning state" << endl;
		m_steering.AutoOrientate = false;
		m_steering.AngularVelocity = ANGLESPEED;
		m_turned += ANGLESPEED * dt;
		if (m_turned >= (PITIMES2)) {
			m_hasTurned = true;
		}
	};

	auto Cooldown = [&]() {
		if (m_isCooldown) {
			m_cdTimer += m_deltaTime;
			if (m_cdTimer >= MAXCDTIMER) {
				m_cdTimer = ZERO;
				m_isCooldown = false;
			}
		}
	};

	auto Grab = [&]() {
		//cout << "grab state" << endl;
		ItemInfo iInfo = {};
		if (m_pInterface->Item_Grab(m_itemInRange, iInfo)) {
			bool pickedUp = false;
			if (iInfo.Type == eItemType::PISTOL)
			{
				cout << "Pistol" << endl;
				if (!m_hasGun) {
					pickedUp = true;

					m_pInterface->Inventory_AddItem(GUN, iInfo);
					m_hasGun = true;
				}
				else {
					ItemInfo currentGun;
					m_pInterface->Inventory_GetItem(GUN, currentGun);
					int currentAmmo = m_pInterface->Item_GetMetadata(currentGun, AMMO);
					int newAmmo = m_pInterface->Item_GetMetadata(iInfo, AMMO);
					if (currentAmmo < newAmmo) {
						m_pInterface->Inventory_RemoveItem(GUN);
						m_pInterface->Inventory_AddItem(GUN, iInfo);
						pickedUp = true;
					}
				}
			}
			else if (iInfo.Type == eItemType::FOOD)
			{
				cout << "Food" << endl;
				bool putFoodInInventory = false;
				if (!m_hasFood) {
					pickedUp = true;
					m_pInterface->Inventory_AddItem(FOOD, iInfo);
					putFoodInInventory = true;
					m_hasFood = true;
				}

				if (!putFoodInInventory) {
					if (!m_hasFood2) {
						pickedUp = true;
						m_pInterface->Inventory_AddItem(FOOD2, iInfo);
						m_hasFood2 = true;
					}
				}
			}
			else if (iInfo.Type == eItemType::MEDKIT)
			{
				cout << "Medkit" << endl;
				if (!m_hasMedkit) {
					pickedUp = true;
					m_pInterface->Inventory_AddItem(MEDKIT, iInfo);
					m_hasMedkit = true;
				}
				else {
					ItemInfo currentMedkit;
					m_pInterface->Inventory_GetItem(MEDKIT, currentMedkit);
					int currentHealth = m_pInterface->Item_GetMetadata(currentMedkit, HEALTH);
					int newHealth = m_pInterface->Item_GetMetadata(iInfo, HEALTH);
					if (currentHealth <= newHealth) {
						m_pInterface->Inventory_UseItem(MEDKIT);
						m_pInterface->Inventory_RemoveItem(MEDKIT);
						m_pInterface->Inventory_AddItem(MEDKIT, iInfo);
						pickedUp = true;
					}
				}
			}
			if (!pickedUp)
			{
				//cout << "Garbage" << endl;
				m_pInterface->Inventory_AddItem(GARBAGE, iInfo);
				m_pInterface->Inventory_RemoveItem(GARBAGE);
			}
		}
		m_pickedUp = true;
	};

	auto setPickupFalse = [&]() {
		m_pickedUp = false;
	};

	auto useMedkit = [&]() {
		//cout << "medkit state" << endl;
		m_pInterface->Inventory_UseItem(MEDKIT);
		m_pInterface->Inventory_RemoveItem(MEDKIT);
		m_hasMedkit = false;
		m_itemUsed = true;
	};

	auto useFood = [&]() {
		//cout << "food state" << endl;
		m_pInterface->Inventory_UseItem(FOOD);
		m_pInterface->Inventory_RemoveItem(FOOD);
		m_hasFood = false;
		m_itemUsed = true;
	};

	auto useFood2 = [&]() {
		//cout << "food2 state" << endl;
		m_pInterface->Inventory_UseItem(FOOD2);
		m_pInterface->Inventory_RemoveItem(FOOD2);
		m_hasFood2 = false;
		m_itemUsed = true;
	};

	auto useGun = [&]() {
		//cout << "Shooting hun state" << endl;
		m_steering.AutoOrientate = false;
		m_steering.AngularVelocity = ANGLESPEED;
		ItemInfo iInfo = {};
		m_pInterface->Inventory_GetItem(GUN, iInfo);
		int ammo = m_pInterface->Item_GetMetadata(iInfo, AMMO);
		if (ammo == ONE) {
			m_pInterface->Inventory_UseItem(GUN);
			m_pInterface->Inventory_RemoveItem(GUN);
			m_hasGun = false;
		}
		else {
			m_pInterface->Inventory_UseItem(GUN);
		}		
		
		m_itemUsed = true;
	};

	auto setHasItemUsed = [&]() {
		m_itemUsed = false;
	};
#pragma endregion
	//Define transition conditions
#pragma region transition
	FSMConditionBase* pConditionFoundHouse = new FSMCondition<>(
		{
			FSMDelegateContainer<bool>(hasHouseInFovAndNotOldHouse)
		});
	m_Conditions.push_back(pConditionFoundHouse);

	FSMConditionBase* pConditionCloseCenter = new FSMCondition<>(
		{
			FSMDelegateContainer<bool>(isCloseToCenter)
		});
	m_Conditions.push_back(pConditionCloseCenter);

	FSMConditionBase* pConditionTurnedAround = new FSMCondition<>(
		{
			FSMDelegateContainer<bool>(beenInHouseFor360Degree)
		});
	m_Conditions.push_back(pConditionTurnedAround);

	FSMConditionBase* pConditionSawEnemy = new FSMCondition<float&>(
		{
			FSMDelegateContainer<bool, float&>(hasEnemyInSight, m_deltaTime)
		});
	m_Conditions.push_back(pConditionSawEnemy);

	FSMConditionBase* pItemInRange = new FSMCondition<>(
		{
			FSMDelegateContainer<bool>(hasItemInRange)
		});
	m_Conditions.push_back(pItemInRange);

	FSMConditionBase* pItemOutRange = new FSMCondition<>(
		{
			FSMDelegateContainer<bool>(hasItemOutOfRange)
		});
	m_Conditions.push_back(pItemOutRange);

	FSMConditionBase* pHasPickup = new FSMCondition<>(
		{
			FSMDelegateContainer<bool>(hasPickUpItem)
		});
	m_Conditions.push_back(pHasPickup);

	FSMConditionBase* pShouldUseMedkit = new FSMCondition<>(
		{
			FSMDelegateContainer<bool>(shouldUseMedkit)
		});
	m_Conditions.push_back(pShouldUseMedkit);

	FSMConditionBase* pShouldUseFood = new FSMCondition<>(
		{
			FSMDelegateContainer<bool>(shouldUseFood)
		});
	m_Conditions.push_back(pShouldUseFood);

	FSMConditionBase* pShouldUseFood2 = new FSMCondition<>(
		{
			FSMDelegateContainer<bool>(shouldUseFood2)
		});
	m_Conditions.push_back(pShouldUseFood2);

	FSMConditionBase* pHasItemUsed = new FSMCondition<>(
		{
			FSMDelegateContainer<bool>(hasItemUsed)
		});
	m_Conditions.push_back(pHasItemUsed);

	FSMConditionBase* pShouldUseGun = new FSMCondition<>(
		{
			FSMDelegateContainer<bool>(shouldUseGun)
		});
	m_Conditions.push_back(pShouldUseGun);
#pragma endregion
	//Define actions
#pragma region actions
	FSMDelegateBase* pSetTargetHouse = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(SetTargetHouse) }
	);
	m_Delegates.push_back(pSetTargetHouse);

	FSMDelegateBase* pSetTargetCheckPoint = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(SetTargetCheckPoint) }
	);
	m_Delegates.push_back(pSetTargetCheckPoint);

	FSMDelegateBase* pSetTargetItem = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(SetTargetItem) }
	);
	m_Delegates.push_back(pSetTargetItem);

	FSMDelegateBase* pMove = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(Move) }
	);
	m_Delegates.push_back(pMove);

	FSMDelegateBase* pTurning = new FSMDelegate<float&>(
		{ FSMDelegateContainer<void, float&>(Turning, m_deltaTime) }
	);
	m_Delegates.push_back(pTurning);

	FSMDelegateBase* pSetCenterHouse = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(setCenterHouse) }
	);
	m_Delegates.push_back(pSetCenterHouse);

	FSMDelegateBase* pSetCenterHouseLastVisited = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(setCenterHouseLastVisited) }
	);
	m_Delegates.push_back(pSetCenterHouseLastVisited);

	FSMDelegateBase* pResetTurning = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(resetTurning) }
	);
	m_Delegates.push_back(pResetTurning);

	FSMDelegateBase* pCooldown = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(Cooldown) }
	);
	m_Delegates.push_back(pCooldown);

	FSMDelegateBase* pGrab = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(Grab) }
	);
	m_Delegates.push_back(pGrab);

	FSMDelegateBase* pSetPickupFalse = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(setPickupFalse) }
	);
	m_Delegates.push_back(pSetPickupFalse);

	FSMDelegateBase* pUseMedkit = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(useMedkit) }
	);
	m_Delegates.push_back(pUseMedkit);

	FSMDelegateBase* pUseFood = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(useFood) }
	);
	m_Delegates.push_back(pUseFood);

	FSMDelegateBase* pUseFood2 = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(useFood2) }
	);
	m_Delegates.push_back(pUseFood2);

	FSMDelegateBase* pUseGun = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(useGun) }
	);
	m_Delegates.push_back(pUseGun);

	FSMDelegateBase* pSetHasItemUsed = new FSMDelegate<>(
		{ FSMDelegateContainer<void>(setHasItemUsed) }
	);
	m_Delegates.push_back(pSetHasItemUsed);
#pragma endregion
	//Define states
	FSMState* pMoveToCheckpointState = new FSMState();
	m_States.push_back(pMoveToCheckpointState);

	FSMState* pMoveToHouseState = new FSMState();
	m_States.push_back(pMoveToHouseState);

	FSMState* pTurningState = new FSMState();
	m_States.push_back(pTurningState);

	FSMState* pGoToItem = new FSMState();
	m_States.push_back(pGoToItem);

	FSMState* pGrabItem = new FSMState();
	m_States.push_back(pGrabItem);

	FSMState* pUseMedkitState = new FSMState();
	m_States.push_back(pUseMedkitState);

	FSMState* pUseFoodState = new FSMState();
	m_States.push_back(pUseFoodState);

	FSMState* pUseFoodState2 = new FSMState();
	m_States.push_back(pUseFoodState2);

	FSMState* pUseGunState = new FSMState();
	m_States.push_back(pUseGunState);

	pMoveToCheckpointState->SetActions({ pSetTargetCheckPoint, pMove, pCooldown });
	pMoveToCheckpointState->SetTransitions({ 
		FSMTransition({ pConditionFoundHouse }, { pSetCenterHouse }, {pMoveToHouseState}),
		FSMTransition({ pShouldUseMedkit },{ },{ pUseMedkitState }),
		FSMTransition({ pShouldUseFood },{},{ pUseFoodState }),
		FSMTransition({ pShouldUseFood2 },{},{ pUseFoodState2 }),
		FSMTransition({ pShouldUseGun },{},{ pUseGunState }),
		FSMTransition({ pItemOutRange },{},{ pGoToItem }) });

	pMoveToHouseState->SetActions({ pSetTargetHouse, pMove });
	pMoveToHouseState->SetTransitions({ 
		FSMTransition({ pConditionCloseCenter },{},{ pTurningState }),  
		FSMTransition({ pItemOutRange },{},{ pGoToItem }),
		FSMTransition({ pShouldUseMedkit },{},{ pUseMedkitState }),
		FSMTransition({ pShouldUseFood },{},{ pUseFoodState }),
		FSMTransition({ pShouldUseFood2 },{},{ pUseFoodState2 }),
		FSMTransition({ pShouldUseGun },{},{ pUseGunState }) });

	pTurningState->SetActions({ pTurning });
	pTurningState->SetTransitions({ 
		FSMTransition({ pConditionTurnedAround, pConditionSawEnemy },{ pSetCenterHouseLastVisited, pResetTurning },{ pMoveToCheckpointState }),  
		FSMTransition({ pItemOutRange },{},{ pGoToItem }),
		FSMTransition({ pShouldUseMedkit },{},{ pUseMedkitState }),
		FSMTransition({ pShouldUseFood },{},{ pUseFoodState }),
		FSMTransition({ pShouldUseFood2 },{},{ pUseFoodState2 }),
		FSMTransition({ pShouldUseGun },{},{ pUseGunState }) });
	
	pGoToItem->SetActions({ pSetTargetItem , pMove});
	pGoToItem->SetTransitions({ 
		FSMTransition({ pItemInRange },{ },{ pGrabItem }),
		FSMTransition({ pShouldUseMedkit },{},{ pUseMedkitState }),
		FSMTransition({ pShouldUseFood },{},{ pUseFoodState }),
		FSMTransition({ pShouldUseFood2 },{},{ pUseFoodState2 }),
		FSMTransition({ pShouldUseGun },{},{ pUseGunState }), });

	pGrabItem->SetActions({pGrab });
	pGrabItem->SetTransitions({ FSMTransition({ pHasPickup },{ pSetPickupFalse },{ pMoveToCheckpointState }) });

	pUseMedkitState->SetActions({ pUseMedkit });
	pUseMedkitState->SetTransitions({ FSMTransition({ pHasItemUsed },{ pSetHasItemUsed },{ pMoveToCheckpointState }) });

	pUseFoodState->SetActions({ pUseFood });
	pUseFoodState->SetTransitions({ FSMTransition({ pHasItemUsed },{ pSetHasItemUsed },{ pMoveToCheckpointState }) });

	pUseFoodState2->SetActions({ pUseFood2 });
	pUseFoodState2->SetTransitions({ FSMTransition({ pHasItemUsed },{ pSetHasItemUsed },{ pMoveToCheckpointState }) });

	pUseGunState->SetActions({ pUseGun });
	pUseGunState->SetTransitions({ FSMTransition({ pHasItemUsed },{ pSetHasItemUsed },{ pMoveToCheckpointState }) });

	m_pStateMachine = new FSM({ pMoveToCheckpointState, pMoveToHouseState, pTurningState}, pMoveToCheckpointState);

	//Boot FSM
	m_pStateMachine->Start();
}

//Called only once
//Called wheb the plugin gets unloaded
void Plugin::DllShutdown()
{
	SAFE_DELETE(m_pStateMachine);
	SAFE_DELETE(m_pSeekBehaviour);

	for (int i = 0; i < static_cast<int>(m_Conditions.size()); ++i)
		SAFE_DELETE(m_Conditions[i]);
	m_Conditions.clear();

	for (int i = 0; i < static_cast<int>(m_Delegates.size()); ++i)
		SAFE_DELETE(m_Delegates[i]);
	m_Delegates.clear();

	for (int i = 0; i < static_cast<int>(m_States.size()); ++i)
		SAFE_DELETE(m_States[i]);
	m_States.clear();
}
	

//Called only once, during initialization
void Plugin::InitGameDebugParams(GameDebugParams& params)
{
	params.AutoFollowCam = true; //Automatically follow the AI? (Default = true)
	params.RenderUI = true; //Render the IMGUI Panel? (Default = true)
	params.SpawnEnemies = true; //Do you want to spawn enemies? (Default = true)
	params.EnemyCount = 20; //How many enemies? (Default = 20)
	params.GodMode = false; //GodMode > You can't die, can be usefull to inspect certain behaviours (Default = false)
							//params.LevelFile = "LevelTwo.gppl";
	params.AutoGrabClosestItem = false; //A call to Item_Grab(...) returns the closest item that can be grabbed. (EntityInfo argument is ignored)
	params.OverrideDifficulty = false; //Override Difficulty?
	params.Difficulty = 1.f; //Difficulty Override: 0 > 1 (Overshoot is possible, >1)
}

//Only Active in DEBUG Mode
//(=Use only for Debug Purposes)
void Plugin::ProcessEvents(const SDL_Event& e)
{

}

//Update
//This function calculates the new SteeringOutput, called once per frame
SteeringPlugin_Output Plugin::UpdateSteering(float dt)
{
	m_deltaTime = dt;
	//auto steering = SteeringPlugin_Output();
	m_steering = SteeringPlugin_Output();
	m_steering.AutoOrientate = false;
	m_steering.AngularVelocity = ANGLESPEED;
	//Use the Interface (IAssignmentInterface) to 'interface' with the AI_Framework
	//auto agentInfo = m_pInterface->Agent_GetInfo();
	m_agent = m_pInterface->Agent_GetInfo();

	if (m_pStateMachine)
		m_pStateMachine->Update();

	return m_steering;	
}

//This function should only be used for rendering debug elements
void Plugin::Render(float dt) const
{
	//This Render function should only contain calls to Interface->Draw_... functions
	//m_pInterface->Draw_SolidCircle(m_Target, .7f, { 0,0 }, { 1, 0, 0 });
}

vector<HouseInfo> Plugin::GetHousesInFOV() const
{
	vector<HouseInfo> vHousesInFOV = {};

	HouseInfo hi = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetHouseByIndex(i, hi))
		{
			vHousesInFOV.push_back(hi);
			continue;
		}

		break;
	}

	return vHousesInFOV;
}

vector<EntityInfo> Plugin::GetEntitiesInFOV() const
{
	vector<EntityInfo> vEntitiesInFOV = {};

	EntityInfo ei = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetEntityByIndex(i, ei))
		{
			vEntitiesInFOV.push_back(ei);
			continue;
		}

		break;
	}

	return vEntitiesInFOV;
}
