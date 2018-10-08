#include "stdafx.h"
#include "SteeringBehaviours.h"

//SEEK
//****
SteeringPlugin_Output Seek::CalculateSteering(float deltaT, AgentInfo* pAgent)
{
	SteeringPlugin_Output steering = {};

	steering.LinearVelocity = (*m_pTargetRef) - pAgent->Position; //Desired Velocity
	steering.LinearVelocity.Normalize(); //Normalize Desired Velocity
	steering.LinearVelocity *= pAgent->MaxLinearSpeed; //Rescale to Max Speed

													   //Keep on turning
	steering.AngularVelocity = m_angle; //Rotate your character to inspect the world while walking
	steering.AutoOrientate = m_rotate; //Setting AutoOrientate to TRue overrides the AngularVelocity

	steering.RunMode = m_run;

	return steering;
}