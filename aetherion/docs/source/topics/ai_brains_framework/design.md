# Design

## Vision


## Scope

The scope of this framework is to have the following abstractions to help organize the project and at the same time put the logic with it's owns responsibilities.

The AIProcessManager will still be the main controller of a Process that manages AI. It will then have many Populations that will have certain porcentage of how many brains we can have.

Also need a way to organize the code for processing the information to the neural networks. Right now there is no preprocessing of the data. For that it needs to be flexible to receive different types of Perceptions

How each of the classes will need interactions with either Python or C++ or both.

## Classes and data flow

### Basics and Genetic Algorithms specifications

#### Genome

This will hold and expose the data part of a genome. Theoretically this should not change throughout the life of an entity. But if the brain depends on the genome to be represented and the brain of certain algorithms will possibly be able to evolve during the life of the creature.

#### Test

test

### Input and input processing of a Brain

#### SensorySystem | Sensorium

This class holds and organizes all the individual perception channels and is responsible for encoding the neural network input.

This will need to be able to parse the following:
`entity_interface.get_perception()`

### Sense

This is the individual building block of a perception (with implementations like Vision, WaterPerception, GroundPerception, etc.).

## Output and postprocessing of the AI

Similarly it will have a collection of Actions and certain implementations of Actions like, walking, consuming item, taking item and so on.

### ActuationSystem

This class is responsible for aggregating all the possible outputs (or “acts”) that your artificial lifeforms can perform. 

### Action

This is the base class for a single action (with concrete implementations such as Walking, ConsumingItem, TakingItem, etc.).

## Brain

### DecisionSystem

Here there will be two possible types of implementation, the neural network or any rigid logic deterministic algorithm that could be implemented.

This will need to be aware of the Gene or Genetic code implementation

This will also determine how the entity will behave during reproduction periods

### BeastBrain

The BeastBrain will have a sensorySystem, something to control the neural network part of the brain. And an Action

## Group of individuals and Genetic Algorithms

## Populations

This will be the class that a genetic algorithm is organized and processed. This will have access or control of the following logic:
* reproduction algorithms and mutations
* Inter population relations (Exchange of soul value - resource distribution)
* Because of that, it will know the Genome type of the 
* Will know how to communicate with the world interface (either client-server or single-process)
* Will have the parallel inferring algorithm to ask inferences for each batch of inferences.
* This will need to have metrics because the Population is the thing that can be considered

The graphical part of implemented the metrics will be handled by another issue, but related to this one.

## Process High-Level Classes

### AIProcessManager

Description:
The AIProcessManager is the central orchestrator for the AI process. It is responsible for managing the lifecycle of the entire system, from individual brains to whole populations.

Responsibilities:

Process Control: Initializes, monitors, and terminates AI processes.
Population Coordination: Manages multiple populations and allocates resources (e.g., brains) appropriately.
Data Flow Management: Coordinates the flow of sensory data, decision-making, and action execution.
Integration Hub: Acts as the primary interface between Python and C++ components, ensuring smooth interoperability. (?)
