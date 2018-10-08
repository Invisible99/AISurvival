#pragma once
#include "IExamPlugin.h"
#include "Exam_HelperStructs.h"
#include "FSM.h"
#include "SteeringBehaviours.h"
class IBaseInterface;
class IExamInterface;

using namespace Elite;
class Plugin :public IExamPlugin
{
public:
	Plugin() {};
	virtual ~Plugin() {};

	void Initialize(IBaseInterface* pInterface, PluginInfo& info) override;
	void DllInit() override;
	void DllShutdown() override;

	void InitGameDebugParams(GameDebugParams& params) override;
	void ProcessEvents(const SDL_Event& e) override;

	SteeringPlugin_Output UpdateSteering(float dt) override;
	void Render(float dt) const override;

private:
	//FSM
	FSM * m_pStateMachine = nullptr;

	//Containers
	std::vector<FSMConditionBase*> m_Conditions = {};
	std::vector<FSMDelegateBase*> m_Delegates = {};
	std::vector<FSMState*> m_States = {};

	Seek* m_pSeekBehaviour = nullptr;

	AgentInfo m_agent;
	EntityInfo m_itemInRange;
	EntityInfo m_itemOutRange;
	bool m_pickedUp = false;
	bool m_hasTurned = false;
	SteeringPlugin_Output m_steering;
	Vector2 m_pointToGo;

	float m_deltaTime;

	enum InventorySlots
	{
		GUN,
		FOOD,
		MEDKIT,
		FOOD2,
		GARBAGE
	};


	//Interface, used to request data from/perform actions with the AI Framework
	IExamInterface* m_pInterface = nullptr;
	vector<HouseInfo> GetHousesInFOV() const;
	vector<EntityInfo> GetEntitiesInFOV() const;
	//Vector2 m_Target = {};
	const float ANGLESPEED = 1.57f; //Demo purpose
	float m_turned = 0.f;
	bool m_turning = false;
	Vector2 m_centerHouseLastVisited = { 0,0 };
	Vector2 m_centerHouseToVisit = { 0,0 };
	Vector2 m_housePoint = { 0,0 };

	const float MAXHEALTH = 10;
	const float MAXENERGY = 10;
	const float MAXCDTIMER = 5;
	bool m_hasGun = false;
	bool m_hasFood = false;
	bool m_hasFood2 = false;
	bool m_hasMedkit = false;
	bool m_itemUsed = false;
	bool m_isCooldown = false;
	float m_cdTimer = 0;

	const int ZEROINT = 0;
	const int ONE = 1;
	const float ZERO = 0.f;
	const float DIVIDE2 = 2.f;
	const float GRABRANGEDIFF = 0.5f;
	const float SHOOTRANGEDIFF = 0.1f;
	const float RUNTO = 4.0f;
	const float PITIMES2 = M_PI * 2;

	const string RANGE = "range";
	const string ENERGY = "energy";
	const string HEALTH = "health";
	const string AMMO = "ammo";
};

//ENTRY
//This is the first function that is called by the host program
//The plugin returned by this function is also the plugin used by the host program
extern "C"
{
	__declspec (dllexport) IPluginBase* Register()
	{
		return new Plugin();
	}
}