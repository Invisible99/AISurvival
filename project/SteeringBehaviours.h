#pragma once
#include "IExamInterface.h"

#pragma region **ISTEERINGBEHAVIOUR** (BASE)
class ISteeringBehaviour
{
public:
	ISteeringBehaviour() {}
	virtual ~ISteeringBehaviour() {}

	virtual SteeringPlugin_Output CalculateSteering(float deltaT, AgentInfo* pAgent) = 0;

	template<class T, typename std::enable_if<std::is_base_of<ISteeringBehaviour, T>::value>::type* = nullptr>
	T* As()
	{
		return static_cast<T*>(this);
	}
};
#pragma endregion


///////////////////////////////////////
//SEEK
//****
class Seek : public ISteeringBehaviour
{
public:
	Seek() {};
	virtual ~Seek() {};

	//Seek Behaviour
	SteeringPlugin_Output CalculateSteering(float deltaT, AgentInfo* pAgent) override;
	void SetAngle(float angle) { m_angle = angle; };
	void SetAutoRotate(bool on) { m_rotate = on; };
	void SetRunning(bool on) { m_run = on; };
	//Seek Functions
	virtual void SetTarget(const Elite::Vector2* pTarget) { m_pTargetRef = pTarget; }

protected:
	const Elite::Vector2* m_pTargetRef = nullptr;
	float m_angle;
	bool m_rotate;
	bool m_run;
};