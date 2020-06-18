﻿/* 
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

#pragma once
//编译一次
#ifdef WIN32
	#define SE_DLL_API __declspec(dllexport) //SE_DLL_API函数的调用接口，导出函数头，在win下
#else
	#define SE_DLL_API  // Leave empty on Mac
#endif

#define SE_NAME_SIZE 32

typedef struct
{
	int id;		        // Automatically generated unique object id 
	int model_id;          // Id to control what 3D model to represent the vehicle - see carModelsFiles_[] in scenarioenginedll.cpp
	int control;	      // 0= undefined, 1=internal, 2=external, 3=hybrid_external, 4=hybrid_ghost
	float timestamp;
	float x;
	float y;
	float z;
	float h;
	float p;
	float r;
	int roadId;
	float t;
	int laneId;
	float laneOffset;
	float s;
	float speed;         //只声明成员变量，不声明函数
} SE_ScenarioObjectState;    //SE_ScenariosObjectState是这个结构体的名称

typedef struct
{
	float x;                // target position, in global coordinate system
	float y;                // target position, in global coordinate system
	float z;                // target position, in global coordinate system
	float heading;		    // road heading at steering target point
	float pitch;		    // road pitch (inclination) at steering target point
	float roll;		        // road roll (camber) at target point
	float curvature;		// road curvature at steering target point
	float speed_limit;		// speed limit given by OpenDRIVE type entry
	float width;            // with of the lane 
} SE_LaneInfo;                 //车道信息

typedef struct
{
	float global_pos_x;     // target position, in global coordinate system
	float global_pos_y;     // target position, in global coordinate system
	float global_pos_z;     // target position, in global coordinate system
	float local_pos_x;      // target position, relative vehicle (pivot position object) coordinate system
	float local_pos_y;      // target position, relative vehicle (pivot position object) coordinate system
	float local_pos_z;      // target position, relative vehicle (pivot position object) coordinate system
	float angle;			// heading angle to target from and relatove to vehicle (pivot position)
	float road_heading;		// road heading at steering target point
	float road_pitch;		// road pitch (inclination) at steering target point
	float road_roll;		// road roll (camber) at target point
	float trail_heading;	// trail heading (only when used for trail lookups, else equals road_heading)
	float curvature;		// road curvature at steering target point
	float speed_limit;		// speed limit given by OpenDRIVE type entry
} SE_RoadInfo;                  //道路信息


#ifdef __cplusplus   		 //如果是cpp文件
extern "C"  			//符合c语言的编译，即本本部分按照c语言的规则进行编译
{
#endif
	/**
	Initialize the scenario engine
	@param oscFilename Path to the OpenSCEANRIO file
	@param control Ego control 0=by OSC 1=Internal 2=External 3=Hybrid
	@param use_viewer 0=no viewer, 1=use viewer
	@param threads 0=single thread, 1=viewer in a separate thread, parallel to scenario engine
	@param record Create recording for later playback 0=no recording 1=recording
	@param headstart_time For hybrid control mode launch ghost vehicle with this headstart time 
	@return 0 if successful, -1 if not
	*/
	SE_DLL_API int SE_Init(const char *oscFilename, int control, int use_viewer, int threads, int record, float headstart_time);

	/**
	Step the simulation forward with specified timestep
	@param dt time step in seconds
	@return 0 if successful, -1 if not
	*/
	SE_DLL_API int SE_StepDT(float dt);

	/**
	Step the simulation forward. Time step will be elapsed system (world) time since last step. Useful for interactive/realtime use cases.
	@return 0 if successful, -1 if not
	*/
	SE_DLL_API int SE_Step();

	/**
	Stop simulation gracefully. Two purposes: 1. Release memory and 2. Prepare for next simulation, e.g. reset object lists.
	*/
	SE_DLL_API void SE_Close();

	/**
	Get simulation time in seconds
	*/
	SE_DLL_API float SE_GetSimulationTime();  // Get simulation time in seconds

	SE_DLL_API int SE_ReportObjectPos(int id, float timestamp, float x, float y, float z, float h, float p, float r, float speed);
	SE_DLL_API int SE_ReportObjectRoadPos(int id, float timestamp, int roadId, int laneId, float laneOffset, float s, float speed);

	SE_DLL_API int SE_GetNumberOfObjects();
	SE_DLL_API int SE_GetObjectState(int index, SE_ScenarioObjectState *state);
	SE_DLL_API int SE_GetObjectGhostState(int index, SE_ScenarioObjectState *state);
	SE_DLL_API int SE_GetObjectStates(int *nObjects, SE_ScenarioObjectState* state);

	/**
	Create an ideal object sensor and attach to specified vehicle
	@param object_id Handle to the object to which the sensor should be attached
	@param x Position x coordinate of the sensor in vehicle local coordinates
	@param y Position y coordinate of the sensor in vehicle local coordinates
	@param z Position z coordinate of the sensor in vehicle local coordinates
	@param h heading of the sensor in vehicle local coordinates
	@param fovH Horizontal field of view, in degrees
	@param rangeNear Near value of the sensor depth range
	@param rangeFar Far value of the sensor depth range
	@param maxObj Maximum number of objects theat the sensor can track
	@return Sensor ID (Global index of sensor), -1 if unsucessful
	*/
	SE_DLL_API int SE_AddObjectSensor(int object_id, float x, float y, float z, float h, float rangeNear, float rangeFar, float fovH, int maxObj);

	/**
	Fetch list of identified objects from a sensor
	@param sensor_id Handle (index) to the sensor
	@param list Array of object indices
	@return Number of identified objects, i.e. length of list. -1 if unsuccesful.
	*/
	SE_DLL_API int SE_FetchSensorObjectList(int sensor_id, int *list);

	/**
	Get information suitable for driver modeling of a point at a specified distance from object along the road ahead
	@param object_id Id of the object from which to measure
	@param lookahead_distance The distance, along the road, to the point
	@param data Struct including all result values, see typedef for details
	@param lookAheadMode Measurement strategy: Along 0=lane center, 1=road center (ref line) or 2=current lane offset. See roadmanager::Position::LookAheadMode enum
	@return 0 if successful, -1 if not
	*/
	SE_DLL_API int SE_GetRoadInfoAtDistance(int object_id, float lookahead_distance, SE_RoadInfo *data, int lookAheadMode);

	/**
	Get road information of a point at a specified distance from object along the road ahead 
	@param object_id Id of the object from which to measure
	@param lookahead_distance The distance, along the road, to the point
	@param data Struct including all result values, see typedef for details
	@param lookAheadMode Measurement strategy: Along 0=lane center, 1=road center (ref line) or 2=current lane offset. See roadmanager::Position::LookAheadMode enum
	@return 0 if successful, -1 if not
	*/
	SE_DLL_API int SE_GetLaneInfoAtDistance(int object_id, float lookahead_distance, SE_LaneInfo *data, int lookAheadMode);

	/**
	Get information suitable for driver modeling of a ghost vehicle driving ahead of the ego vehicle
	@param object_id Handle to the position object from which to measure (the actual externally controlled Ego vehicle, not ghost)
	@param lookahead_distance The distance, along the ghost trail, to the point from the current Ego vehicle location
	@param data Struct including all result values, see typedef for details
	@param speed_ghost reference to a variable returning the speed that the ghost had at this point along trail
	@return 0 if successful, -1 if not
	*/
	SE_DLL_API int SE_GetRoadInfoAlongGhostTrail(int object_id, float lookahead_distance, SE_RoadInfo *data, float *speed_ghost);

	
#ifdef __cplusplus
}
#endif
