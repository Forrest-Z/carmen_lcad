#include "udatmo.h"

#include "detector.h"

#include <stdexcept>
#include <carmen/global_graphics.h>

extern carmen_mapper_virtual_laser_message virtual_laser_message;

using udatmo::Detector;

static Detector *detector = NULL;
static Detector *detector_center = NULL;
static Detector *detector_left = NULL;
static Detector *detector_right = NULL;

void udatmo_init(const carmen_robot_ackerman_config_t robot_config)
{
	if (detector != NULL)
		throw std::runtime_error("uDATMO module already initialized");

	detector = new Detector(robot_config);
	detector_center = new Detector(robot_config);
	detector_left = new Detector(robot_config);
	detector_right = new Detector(robot_config);
}

bool udatmo_obstacle_detected(double timestamp)
{
	static double last_obstacle_detected_timestamp = 0.0;

	if (detector->detected)
		last_obstacle_detected_timestamp = timestamp;

	if (fabs(timestamp - last_obstacle_detected_timestamp) < 3.0)
		return (true);
	else
		return (false);
}

void udatmo_clear_detected(void)
{
	detector->detected = false;
	detector_center->detected = false;
	detector_left->detected = false;
	detector_right->detected = false;
}

void udatmo_shift_history(void)
{
	detector->shift();
	detector_center->shift();
	detector_left->shift();
	detector_right->shift();
}

int udatmo_detect_obstacle_index(carmen_obstacle_distance_mapper_map_message *current_map,
							carmen_rddf_road_profile_message *rddf,
							int goal_index,
							int rddf_pose_index,
							carmen_ackerman_traj_point_t robot_pose,
							double timestamp)
{
	int index = detector->detect(current_map, rddf, goal_index, rddf_pose_index, robot_pose,
			detector->robot_config.obstacle_avoider_obstacles_safe_distance, 0.0, timestamp);

	if (index != -1)
		return (index);
//	return (-1);

//	bool near_barrier = false;
//	for (int i = 0; i < rddf->number_of_poses; i++)
//	{
//		if (rddf->annotations[i] == RDDF_ANNOTATION_TYPE_BARRIER)
//		{
//			near_barrier = true;
//			break;
//		}
//	}

	int index_left = detector_left->detect(current_map, rddf, goal_index, rddf_pose_index, robot_pose,
			detector->robot_config.model_predictive_planner_obstacles_safe_distance,
			detector->robot_config.model_predictive_planner_obstacles_safe_distance, timestamp);
	int index_right = detector_right->detect(current_map, rddf, goal_index, rddf_pose_index, robot_pose,
			detector->robot_config.model_predictive_planner_obstacles_safe_distance,
			-detector->robot_config.model_predictive_planner_obstacles_safe_distance, timestamp);
	int index_center = detector_center->detect(current_map, rddf, goal_index, rddf_pose_index, robot_pose,
			detector->robot_config.model_predictive_planner_obstacles_safe_distance + 0.4,
			0.0, timestamp);

	if ((index_center != -1) && (index_left != -1))// && !near_barrier)
	{
		detector->copy_state(detector_left);
//		for (int i = 0; i < 5; i++)
//		{
//			virtual_laser_message.positions[virtual_laser_message.num_positions].x = detector->moving_object[i].pose.x;
//			virtual_laser_message.positions[virtual_laser_message.num_positions].y = detector->moving_object[i].pose.y;
//			virtual_laser_message.colors[virtual_laser_message.num_positions] = CARMEN_RED;
//			virtual_laser_message.num_positions++;
//
//			virtual_laser_message.positions[virtual_laser_message.num_positions].x = rddf->poses[detector->moving_object[i].rddf_pose_index].x;
//			virtual_laser_message.positions[virtual_laser_message.num_positions].y = rddf->poses[detector->moving_object[i].rddf_pose_index].y;
//			virtual_laser_message.colors[virtual_laser_message.num_positions] = CARMEN_GREEN;
//			virtual_laser_message.num_positions++;
//		}
		return (index_left);
	}

	if ((index_center != -1) && (index_right != -1))// && !near_barrier)
	{
		detector->copy_state(detector_right);
//		for (int i = 0; i < 5; i++)
//		{
//			virtual_laser_message.positions[virtual_laser_message.num_positions].x = detector->moving_object[i].pose.x;
//			virtual_laser_message.positions[virtual_laser_message.num_positions].y = detector->moving_object[i].pose.y;
//			virtual_laser_message.colors[virtual_laser_message.num_positions] = CARMEN_RED;
//			virtual_laser_message.num_positions++;
//
//			virtual_laser_message.positions[virtual_laser_message.num_positions].x = rddf->poses[detector->moving_object[i].rddf_pose_index].x;
//			virtual_laser_message.positions[virtual_laser_message.num_positions].y = rddf->poses[detector->moving_object[i].rddf_pose_index].y;
//			virtual_laser_message.colors[virtual_laser_message.num_positions] = CARMEN_GREEN;
//			virtual_laser_message.num_positions++;
//		}
		return (index_right);
	}

	return (-1);
}

double udatmo_speed_front(void)
{
	return detector->speed_front();
}

double udatmo_speed_left(void)
{
	return detector_left->speed_front();
}

double udatmo_speed_right(void)
{
	return detector_right->speed_front();
}

double udatmo_speed_center(void)
{
	return detector_center->speed_front();
}

carmen_ackerman_traj_point_t udatmo_get_moving_obstacle_position(void)
{
	return detector->get_moving_obstacle_position();
}

double udatmo_get_moving_obstacle_distance(carmen_ackerman_traj_point_t robot_pose, carmen_robot_ackerman_config_t *robot_config)
{
	double distance = detector->get_moving_obstacle_distance(robot_pose) - (robot_config->distance_between_front_and_rear_axles + robot_config->distance_between_front_car_and_front_wheels);

	return (distance);
}
