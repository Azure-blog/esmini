/* 
 * esmini - Environment Simulator Minimalistic 
 * https://github.com/esmini/esmini
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * 
 * Copyright (c) partners of Simulation Scenarios
 * https://sites.google.com/view/simulationscenarios
 */

#include "ScenarioEngine.hpp"
#include "CommonMini.hpp"

using namespace scenarioengine;

ScenarioEngine::ScenarioEngine(std::string oscFilename, double headstart_time, RequestControlMode control_mode_first_vehicle)
{
	InitScenario(oscFilename, headstart_time, control_mode_first_vehicle);
}

ScenarioEngine::ScenarioEngine(const pugi::xml_document &xml_doc, double headstart_time, RequestControlMode control_mode_first_vehicle)
{
	InitScenario(xml_doc, headstart_time, control_mode_first_vehicle);
}

void ScenarioEngine::InitScenario(std::string oscFilename, double headstart_time, RequestControlMode control_mode_first_vehicle)
{
	// Load and parse data
	LOG("Init %s", oscFilename.c_str());
	headstart_time_ = headstart_time;
	if (scenarioReader.loadOSCFile(oscFilename.c_str()) != 0)
	{
		throw std::invalid_argument(std::string("Failed to load OpenSCENARIO file ") + oscFilename);
	}

	parseScenario(control_mode_first_vehicle);
}

void ScenarioEngine::InitScenario(const pugi::xml_document &xml_doc, double headstart_time, RequestControlMode control_mode_first_vehicle)
{
	scenarioReader.loadOSCMem(xml_doc);
	parseScenario(control_mode_first_vehicle);
}

ScenarioEngine::~ScenarioEngine()
{
	LOG("Closing");
}

void ScenarioEngine::step(double deltaSimTime, bool initial)	
{
	simulationTime += deltaSimTime;

	if (entities.object_.size() == 0)
	{
		return;
	}	

	// Fetch external states from gateway, except the initial run where scenario engine sets all positions
	if (!initial)
	{
		for (size_t i = 0; i < entities.object_.size(); i++)
		{
			if (entities.object_[i]->control_ == Object::Control::EXTERNAL ||
				entities.object_[i]->control_ == Object::Control::HYBRID_EXTERNAL)
			{
				ObjectState o;

				if (scenarioGateway.getObjectStateById(entities.object_[i]->id_, o) != 0)
				{
					LOG("Gateway did not provide state for external car %d", entities.object_[i]->id_);
				}
				else
				{
					entities.object_[i]->pos_ = o.state_.pos;
					entities.object_[i]->speed_ = o.state_.speed;
					entities.object_[i]->wheel_angle = o.state_.wheel_angle;
				}
			}
		}
	}

	// Kick off initial actions
	if (initial)
	{
		// kick off init actions
		for (size_t i = 0; i < init.private_action_.size(); i++)
		{
			init.private_action_[i]->Trig();
		}
	}

	// Step inital actions - might be extened in time (more than one step)
	for (size_t i = 0; i < init.private_action_.size(); i++)
	{
		if (init.private_action_[i]->IsActive())
		{
			//LOG("Stepping action of type %d", init.private_action_[i]->action_[j]->type_)
			init.private_action_[i]->Step(deltaSimTime);
		}
	}

	// Story 
	for (size_t i = 0; i < story.size(); i++)
	{
		for (size_t j = 0; j < story[i]->act_.size(); j++)
		{
			// Update deactivated elements' state to inactive - This could probably be done in some other way...
			for (size_t k = 0; k < story[i]->act_[j]->sequence_.size(); k++)
			{
				for (size_t l = 0; l < story[i]->act_[j]->sequence_[k]->maneuver_.size(); l++)
				{
					OSCManeuver *maneuver = story[i]->act_[j]->sequence_[k]->maneuver_[l];

					for (size_t m = 0; m < maneuver->event_.size(); m++)
					{
						for (size_t n = 0; n < maneuver->event_[m]->action_.size(); n++)
						{
							if (maneuver->event_[m]->action_[n]->state_ == OSCAction::State::DEACTIVATED)
							{
								maneuver->event_[m]->action_[n]->state_ = OSCAction::State::INACTIVE;
							}
						}
						if (maneuver->event_[m]->state_ == Event::State::DEACTIVATED)
						{
							maneuver->event_[m]->state_ = Event::State::INACTIVE;
						}
					}
				}
			}
			if (story[i]->act_[j]->state_ == Act::State::DEACTIVATED)
			{
				story[i]->act_[j]->state_ = Act::State::INACTIVE;
			}

			// Check Act conditions
			if (!story[i]->act_[j]->IsActive())
			{
				// Check start conditions
				for (size_t k = 0; k < story[i]->act_[j]->start_condition_group_.size(); k++)
				{
					for (size_t l = 0; l < story[i]->act_[j]->start_condition_group_[k]->condition_.size(); l++)
					{
						if (story[i]->act_[j]->start_condition_group_[k]->condition_[l]->Evaluate(story[i], simulationTime))
						{
							story[i]->act_[j]->Trig();
						}
					}
				}
			}
			else
			{
				// If activated last step, make transition to activated
				if (story[i]->act_[j]->state_ == Act::State::ACTIVATED)
				{
					story[i]->act_[j]->state_ = Act::State::ACTIVE;
				}
			}

			if (story[i]->act_[j]->IsActive())
			{
				// Check end conditions
				for (size_t k = 0; k < story[i]->act_[j]->end_condition_group_.size(); k++)
				{
					for (size_t l = 0; l < story[i]->act_[j]->end_condition_group_[k]->condition_.size(); l++)
					{
						if (story[i]->act_[j]->end_condition_group_[k]->condition_[l]->Evaluate(story[i], simulationTime))
						{
							story[i]->act_[j]->Stop();
						}
					}
				}

				// Check cancel conditions
				for (size_t k = 0; k < story[i]->act_[j]->cancel_condition_group_.size(); k++)
				{
					for (size_t l = 0; l < story[i]->act_[j]->cancel_condition_group_[k]->condition_.size(); l++)
					{
						if (story[i]->act_[j]->cancel_condition_group_[k]->condition_[l]->Evaluate(story[i], simulationTime))
						{
							story[i]->act_[j]->Stop();
						}
					}
				}
			}

			// Maneuvers
			if (story[i]->act_[j]->IsActive())
			{
				for (size_t k = 0; k < story[i]->act_[j]->sequence_.size(); k++)
				{
					for (size_t l = 0; l < story[i]->act_[j]->sequence_[k]->maneuver_.size(); l++)
					{
						OSCManeuver *maneuver = story[i]->act_[j]->sequence_[k]->maneuver_[l];

						// Events - may only execute one at a time
						for (size_t m = 0; m < maneuver->event_.size(); m++)
						{
							Event *event = story[i]->act_[j]->sequence_[k]->maneuver_[l]->event_[m];

							if (event->IsActive())
							{
								// If just activated, make transition to active
								if (event->state_ == (Event::State)Act::State::ACTIVATED)
								{
									event->state_ = Event::State::ACTIVE;
								}
							}
							else if (event->state_ == (Event::State)Act::State::DEACTIVATED)
							{
								// If just deactivated, make transition to inactive
								event->state_ = Event::State::INACTIVE;
							}
						}
						if (maneuver->GetActiveEventIdx() == -1 && maneuver->GetWaitingEventIdx() >= 0)
						{
							// When no active event, it's OK to trig waiting event
							maneuver->event_[maneuver->GetWaitingEventIdx()]->Trig();
						}


						for (size_t m = 0; m < maneuver->event_.size(); m++)
						{
							Event *event = maneuver->event_[m];

							if (event->Triggable())
							{
								// Check event conditions
								for (size_t n = 0; n < event->start_condition_group_.size(); n++)
								{
									for (size_t o = 0; o < event->start_condition_group_[n]->condition_.size(); o++)
									{
										if (event->start_condition_group_[n]->condition_[o]->Evaluate(story[i], simulationTime))
										{
											// Check priority
											if (event->priority_ == Event::Priority::OVERWRITE)
											{
												// Deactivate any currently active event
												if (maneuver->GetActiveEventIdx() >= 0)
												{
													LOG("Event %s cancelled", maneuver->event_[maneuver->GetActiveEventIdx()]->name_.c_str());
													maneuver->event_[maneuver->GetActiveEventIdx()]->Stop();
												}

												// Activate trigged event
												event->Trig();
											}
											else if (event->priority_ == Event::Priority::FOLLOWING)
											{
												// If already an active event, this event will wait
												if (maneuver->GetActiveEventIdx() >= 0)
												{
													event->state_ = Event::State::WAITING;
													LOG("Event %s is running, trigged event %s is waiting",
														maneuver->event_[maneuver->GetActiveEventIdx()]->name_.c_str(), event->name_.c_str());
												}
												else
												{
													event->Trig();
												}
											}
											else if (event->priority_ == Event::Priority::SKIP)
											{
												if (maneuver->GetActiveEventIdx() >= 0)
												{
													LOG("Event %s is running, skipping trigged %s",
														maneuver->event_[maneuver->GetActiveEventIdx()]->name_.c_str(), event->name_.c_str());
												}
												else
												{
													event->Trig();
												}
											}
											else
											{
												LOG("Unknown event priority: %d", event->priority_);
											}
										}
									}
								}
							}

							// Update (step) all active actions, for all objects connected to the action
							if (event->IsActive())
							{
								bool active = false;

								for (size_t n = 0; n < event->action_.size(); n++)
								{
									//LOG("action %s state 1: %d", event->action_[n]->name_.c_str(), event->action_[n]->state_);
									//LOG("action %s state 2: %d", event->action_[n]->name_.c_str(), event->action_[n]->state_);
									if (event->action_[n]->state_ == OSCAction::State::TRIGGED)
									{
										event->action_[n]->state_ = OSCAction::State::ACTIVATED;
									}
									else if (event->action_[n]->state_ == OSCAction::State::ACTIVATED)
									{
										event->action_[n]->state_ = OSCAction::State::ACTIVE;
									}

									if (event->action_[n]->IsActive())
									{
										event->action_[n]->Step(deltaSimTime);
										active = active || (event->action_[n]->IsActive());
									}
								}
								if (!active)
								{
									// Actions done -> Set event done
									event->Stop();
								}
							}
						}
					}
				}
			}
		}
	}

	// Report resulting states to the gateway
	for (size_t i = 0; i < entities.object_.size(); i++)
	{
		Object *obj = entities.object_[i];
		
		if (initial)
		{
#if 0
			// Report all scenario objects the initial run, to establish initial positions and speed = 0
			scenarioGateway.reportObject(ObjectState(obj->id_, obj->name_, obj->model_id_, obj->control_, simulationTime, 
				&obj->pos_, 0.0, 0.0, obj->ghost_ ? obj->ghost_->id_ : -1));
#else
			// Report all scenario objects the initial run, to establish initial positions and speed = 0
			scenarioGateway.reportObject(ObjectState(obj->id_, obj->name_, obj->model_id_, obj->control_, simulationTime,
				&obj->pos_, 0.0, 0.0));
#endif
		}
		else if (obj->control_ == Object::Control::INTERNAL ||
			obj->control_ == Object::Control::HYBRID_GHOST)
		{
#if 0
			// Then report all except externally controlled objects
			scenarioGateway.reportObject(ObjectState(obj->id_, obj->name_, obj->model_id_, obj->control_, simulationTime, 
				&obj->pos_, obj->speed_, obj->wheel_angle, -1));
#else
			// Then report all except externally controlled objects
			scenarioGateway.reportObject(ObjectState(obj->id_, obj->name_, obj->model_id_, obj->control_, simulationTime,
				&obj->pos_, obj->speed_, obj->wheel_angle));
#endif
		}
	}

	stepObjects(deltaSimTime);
}

void ScenarioEngine::printSimulationTime()
{
	LOG("simulationTime = %.2f", simulationTime);
}

ScenarioGateway *ScenarioEngine::getScenarioGateway()
{
	return &scenarioGateway;
}

Object::Control ScenarioEngine::GetControl()
{
	if (entities.object_.size() > 0)
	{
		return entities.object_[0]->control_;
	}

	LOG("No objects initialized yet - ask later");

	return Object::Control::UNDEFINED;  // Hmm, what is a good default value...?
}

void ScenarioEngine::SetControl(RequestControlMode control)
{
	if (entities.object_.size() > 0)
	{
		if (control == CONTROL_INTERNAL)
		{
			entities.object_[0]->control_ = Object::Control::INTERNAL;
		}
		else if (control == CONTROL_EXTERNAL)
		{
			entities.object_[0]->control_ = Object::Control::EXTERNAL;
		}
		else if (control == CONTROL_HYBRID)
		{
			entities.object_[0]->control_ = Object::Control::HYBRID_GHOST;
		}
		else 
		{
			LOG("Unexpected requested control mode: %d - falling back to default (INTERNAL)");
			entities.object_[0]->control_ = Object::Control::INTERNAL;
		}
	}
}

void ScenarioEngine::ResolveHybridVehicles()
{
	// Identify any hybrid objects. Make it ghost and create an externally controlled buddy
	size_t num_objects = entities.object_.size();
	for (size_t i = 0; i < num_objects; i++)
	{
		if (entities.object_[i]->control_ == Object::Control::HYBRID_GHOST)
		{
			// Create a vehicle for external control
			Vehicle *external_vehicle = new Vehicle();

			// Copy all properties from the ghost
			*external_vehicle = *(Vehicle*)entities.object_[i];

			// Add "_ghost" to original vehicle name
			entities.object_[i]->name_.append("_ghost");

			// Adjust some properties for the externally controlled buddy
			external_vehicle->control_ = Object::Control::HYBRID_EXTERNAL;
			// Connect external vehicle to the ghost
			external_vehicle->ghost_ = entities.object_[i];

			entities.object_[i]->id_ = (int)entities.object_.size();
			entities.object_.push_back(entities.object_[i]);
			entities.object_[i] = external_vehicle;
			entities.object_[i]->id_ = (int)i;

			// Switch position of the vehicles so that the externally controlled one takes ghost's places

//			entities.object_[i]->external_ = external_vehicle->ghost_;
		}
	}

	for (size_t i = 0; i < entities.object_.size(); i++)
	{
		LOG("i %d id %d mode %d ghost id %d", i, entities.object_[i]->id_, entities.object_[i]->control_,
			entities.object_[i]->ghost_ ? entities.object_[i]->ghost_->id_ : -1);
	}

}

void ScenarioEngine::parseScenario(RequestControlMode control_mode_first_vehicle)
{
	bool hybrid_objects = false;

	// Init road manager
	scenarioReader.parseRoadNetwork(roadNetwork);
	roadmanager::Position::LoadOpenDrive(getOdrFilename().c_str());
	odrManager = roadmanager::Position::GetOpenDrive();

	scenarioReader.parseParameterDeclaration();
	scenarioReader.parseCatalogs(catalogs, &entities);
	scenarioReader.parseEntities(entities, &catalogs);
	// Possibly override control mode of first vehicle
	if (control_mode_first_vehicle != CONTROL_BY_OSC)
	{
		SetControl(control_mode_first_vehicle);
	}
	ResolveHybridVehicles();
	scenarioReader.parseInit(init, &entities, &catalogs);
	scenarioReader.parseStory(story, &entities, &catalogs);

	// Copy init actions from external buddy
	// (Cloning of story actions are handled in the story parser)

	size_t num_private_actions = init.private_action_.size();
	for (size_t i = 0; i < num_private_actions; i++)
	{
		if (init.private_action_[i]->object_->ghost_)
		{
			OSCPrivateAction *paction = init.private_action_[i]->Copy();
			paction->object_ = init.private_action_[i]->object_->ghost_;
			init.private_action_.push_back(paction);
		}
	}

	
	for (size_t i = 0; i < entities.object_.size(); i++)
	{
		if (entities.object_[i]->control_ == Object::Control::HYBRID_GHOST)
		{
			hybrid_objects = true;
			break;
		}
	}

	if (hybrid_objects)
	{
		this->simulationTime = -headstart_time_;
	}
	else
	{
		this->simulationTime = 0;
	}

	// Print loaded data
	entities.Print();

	for (size_t i = 0; i < story.size(); i++)
	{
		story[i]->Print();
	}
}

void ScenarioEngine::stepObjects(double dt)
{
	for (size_t i = 0; i < entities.object_.size(); i++)
	{
		Object *obj = entities.object_[i];

		if ((simulationTime > 0 && obj->control_ == Object::Control::INTERNAL) ||
			obj->control_ == Object::Control::HYBRID_GHOST)
		{
			double steplen = obj->speed_ * dt;

			// Adjustment movement to heading and road direction 
			if (GetAbsAngleDifference(obj->pos_.GetH(), obj->pos_.GetDrivingDirection()) > M_PI_2)
			{
				// If pointing in other direction 
				steplen *= -1;
			}
			if (obj->pos_.GetRoute())
			{
				obj->pos_.MoveRouteDS(steplen);
			}
			else
			{
				obj->pos_.MoveAlongS(steplen);
			}
		}
	}
}

