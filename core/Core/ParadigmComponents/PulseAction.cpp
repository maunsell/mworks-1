/*
 *  PulseAction.cpp
 *  MWorksCore
 *
 *  Created by bkennedy on 11/24/08.
 *  Copyright 2008 mit. All rights reserved.
 *
 */

#include "PulseAction.h"


BEGIN_NAMESPACE_MW


Pulse::Pulse(shared_ptr<Variable> _var, shared_ptr<Variable> _duration) : var(_var) {
	setName("Pulse");
	one_variable = 
	shared_ptr<ConstantVariable>(new ConstantVariable(1.0));
	
	zero_variable = 
	shared_ptr<ConstantVariable>(new ConstantVariable(0.0));
	
	actions = 
	shared_ptr<ScheduledActions>(new ScheduledActions(
														one_variable, _duration,
														zero_variable, zero_variable));
	
	assignment = 
	shared_ptr<Assignment>(new Assignment(var, zero_variable));
	
	actions->addAction(assignment);
}

Pulse::~Pulse(){}

bool Pulse::execute(){
	(*var) = 1L;
	return actions->execute();
}

shared_ptr<mw::Component> PulseFactory::createObject(std::map<std::string, std::string> parameters,
												   ComponentRegistry *reg) {
	REQUIRE_ATTRIBUTES(parameters, "variable", "duration");
	
	shared_ptr<Variable> variable = reg->getVariable(parameters.find("variable")->second);
	shared_ptr<Variable> duration = reg->getVariable(parameters.find("duration")->second);
	
	
	checkAttribute(duration, parameters["reference_id"], "duration", parameters.find("duration")->second);
	
	
	checkAttribute(variable, parameters["reference_id"], "variable", parameters.find("variable")->second);
	
	
	shared_ptr <mw::Component> newPulseAction = shared_ptr<mw::Component>(new Pulse(variable, duration));
	return newPulseAction;		
}


END_NAMESPACE_MW
