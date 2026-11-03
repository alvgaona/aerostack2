# Bugs and pitfalls found while writing the docs

Found on 2026-11-02 while rewriting the documentation site. Not filed as issues yet.

## Three of the five alerts do nothing

`as2_msgs/msg/AlertEvent.msg` says negative alerts are handled by the platform and positive ones by
the framework. In practice only two of the five are acted on:

| Alert | Value | Python API method | What happens |
| --- | --- | --- | --- |
| `KILL_SWITCH` | -1 | `send_emergency_killswitch_to_aircraft()` | Platform enters `EMERGENCY` and calls `ownKillSwitch()` |
| `EMERGENCY_HOVER` | -2 | `send_emergency_hover_to_aircraft()` | Platform enters `EMERGENCY` and calls `ownStopPlatform()` |
| `EMERGENCY_LAND` | -3 | `send_emergency_land_to_aircraft()` | **Nothing.** Falls through to `default: break` |
| `FORCE_HOVER` | +1 | `send_emergency_hover()` | **Nothing.** No subscriber acts on it |
| `FORCE_LAND` | +2 | `send_emergency_land()` | **Nothing**, although it logs "Starting emergency landing." |

Where:

- `as2_core/src/aerial_platform.cpp`, `AerialPlatform::alertEvent()`: returns early for positive
  alerts and only has cases for `KILL_SWITCH` and `EMERGENCY_HOVER`. This has been the case since
  the first commit (e3dc1ca2, 2022-12-09).
- `alertEvent()` is not virtual, so a platform cannot add `EMERGENCY_LAND` handling itself.
- `as2_python_api/as2_python_api/drone_interface_base.py`: the three methods above publish alerts
  that nothing handles. Their names and log messages suggest they act.
- The only consumer of positive alerts is the behavior tree's `WaitForAlert` decorator, which passes
  any alert value to the tree. Nothing reacts by default.

Why it matters: a user who calls `send_emergency_land()` during an incident expects the drone to
land, and it doesn't.

Possible fix:

1. Handle `EMERGENCY_LAND` in the platform, using `ownLand()` when the platform supports native
   landing and a fallback otherwise. Or make the handler virtual, or add an `ownEmergencyLand()` hook.
2. Decide who owns `FORCE_HOVER` and `FORCE_LAND`, for example the controller switching to hover
   and the land behavior taking over.
3. Until then, warn in the docstrings of the three Python methods.

## Control mode numbers differ between the message and the platform config files

`as2_msgs/msg/ControlMode.msg` numbers the modes `HOVER = 1`, `POSITION = 2`, `SPEED = 3`,
`SPEED_IN_A_PLANE = 4`, `ATTITUDE = 5`, `BODY_RATES = 6`, `TRAJECTORY = 7`.

The platform `control_modes.yaml` files (for example
`as2_aerial_platforms/as2_platform_multirotor_simulator/config/control_modes.yaml`) use a bit
encoding with different numbers: `hover = 1`, `body_rates = 2`, `attitude = 3`, `speed = 4`,
`speed_in_a_plane = 5`, `position = 6`, `trajectory = 7`.

Position is 2 in the message and 6 in the file; body rates is 6 in the message and 2 in the file.
Not a runtime bug as long as each side is decoded with its own table, but an easy trap when writing a
platform or reading a config by hand. Worth documenting next to the file format, or unifying.

## No way out of the EMERGENCY state

`as2_core/src/platform_state_machine.cpp` defines transitions into `EMERGENCY` from every state but
none out of it. Once a platform is in emergency it keeps calling `ownStopPlatform()` on every command
cycle until it is restarted. This may be intended, but it isn't documented.

## A cancelled take-off or landing leaves the platform in EMERGENCY

`TakeoffBehavior::on_execution_end()` and `LandBehavior::on_execution_end()` in
`as2_behaviors/as2_behaviors_motion` send `TOOK_OFF` or `LANDED` on success, and `EMERGENCY` on any
other outcome, including a cancel or an abort. Combined with the section above, cancelling a
take-off leaves the platform permanently in emergency, sending stop commands until it is restarted.
A cancel probably shouldn't be treated like a failure, or the state machine needs a way back.

## Jazzy CI skips two packages

`.github/workflows/build-jazzy.yaml` builds every package except `as2_behaviors_param_estimation`
and `as2_behaviors_payload` (and the `aerostack2` metapackage), which the Humble workflow includes.
Unclear whether they fail on Jazzy or were left out by accident.

## Native take-off and landing send their final event twice

With `takeoff_plugin_platform`, `AerialPlatform::takeoff()` sends `TOOK_OFF` itself when `ownTakeoff()`
succeeds, and then `TakeoffBehavior::on_execution_end()` sends `TOOK_OFF` again on success. The
second one is an invalid transition (`FLYING` + `TOOK_OFF`), so the behavior logs "Could not set FSM
to Took OFF" after every successful take-off. `land()` and `land_plugin_platform` do the same with
`LANDED`. Harmless, but the error log is misleading. One of the two should send the event.

## The connected flag is always true

`AerialPlatform` sets `platform_info_msg_.connected = true` in its constructor with a
`TODO(miferco97): Check if connected`, and nothing in the base class changes it. `sendCommand()` checks
it, so the check never blocks unless a platform sets the flag itself.

## Python `resume()` blocks by default

`BehaviorHandler.resume(self, wait_result: bool = True)` in
`as2_python_api/as2_python_api/behavior_actions/behavior_handler.py` waits until the behavior
finishes, while `pause()` and `stop()` return right away. A mission that pauses a behavior, resumes
it and then wants to modify or stop it hangs on `resume()` until the behavior ends. It surprises
anyone who started the behavior with `wait=False`. Defaulting `wait_result` to `False`, or to
whatever the behavior was started with, would match the other calls.

## as2_behavior README describes the wrong service type

`as2_behaviors/as2_behavior/README.md` says pause, resume and stop use `std_srvs::srv::Empty`. The
server creates them as `std_srvs::srv::Trigger` (`behavior_server__impl.hpp`), which also returns a
success flag and a message. Documentation only.

## Gazebo drones are rooted at the bare namespace, not `<ns>/base_link`

Design issue, not a crash. Every other platform has the drone's body frame at `<ns>/base_link`.
Gazebo drones need `base_frame_id: ""` in the state estimator so the body frame resolves to the bare
namespace (`drone0`), as `project_gazebo/config/config.yaml` does.

Why: the drone is a Gazebo model named after the namespace, and its body link is already called
`base_link` (`as2_gazebo_assets/models/quadrotor_base/quadrotor_base.sdf`). The `PosePublisher`
system in `quadrotor_base.sdf.jinja` (with `publish_model_pose` false) publishes
`drone0 -> drone0/base_link` and the payload sensors hang off `drone0`, which has no parent. If the
estimator published `odom -> drone0/base_link`, that frame would have two parents and the sensors
would sit in a separate tree. Publishing `odom -> drone0` joins them, and works because `base_link`
sits at the model origin.

Why it matters: Gazebo configs don't carry over to real drones as they are, the docs need a special
case, and REP 105/120 expect `base_link` as the body frame.

Possible fixes, both inside `as2_gazebo_assets`:

1. Relay the Gazebo poses in the bridge with the model frame renamed to `<ns>/base_link` and drop
   the redundant `drone0 -> drone0/base_link` transform. Sensors then hang off `<ns>/base_link` and
   the estimator keeps its default. Smallest change.
2. Set sensor frame ids in the SDF (`gz_frame_id` on Harmonic, `ignition_frame_id` on Fortress) and
   publish sensor mounts relative to `base_link`. Cleaner, but touches every sensor model and differs
   per Gazebo version.

The empty-name resolution in `as2::Node` can stay for anyone who needs it. Not verified on a live
TF tree yet: check with `ros2 run tf2_tools view_frames` first.

## External platform bugs found while writing the docs

Found by reading the code in each platform repo at its 2026-08-18 commit. None of these were run on
hardware.

- **mavlink**: the kill switch dereferences `mavlink_command_long_client_`, which is never created,
  so it likely crashes the node. The docs say the kill switch doesn't work yet. UNSET is listed in
  `control_modes.yaml` but rejected by the code.
- **mavlink, pixhawk, betaflight**: each overrides `sendCommand` and skips the base checks for
  armed, offboard and EMERGENCY. Pixhawk and MAVLink stream body rates at `min_thrust` while armed
  without a mode.
- **pixhawk**: `resetRatesSetpoint` writes the attitude setpoint's thrust, and the ATTITUDE disarm
  branch writes the rates setpoint's thrust. The battery warning check `warning >= 0` is always true.
  `manual_from_operator_` is never reset after the pilot leaves offboard. README says battery and
  kill switch aren't available, but the code uses both.
- **crazyflie**: battery percentage is on a 0-100 scale, BatteryState expects 0-1. Yaw rate is
  negated for speed but not for speed in a plane. The shipped swarm file enables the AI deck for cf1
  at a lab IP.
- **dji_psdk**: kill switch and stop only release control authority, while the header promises a
  motor stop and a zero setpoint. HOVER ends up in the yaw-angle branch and sends nothing.
  `ctl_authority_` is uninitialized. Angular velocity in odom is always zero. The twist rotation may
  be inverted.
- **dji_osdk**: in yaw-angle mode the command is `-yaw` with no +90 degree offset, while odom uses
  `-yaw + pi/2`, which looks like a 90 degree heading mismatch. Gimbal degrees are converted as if
  they were radians. Kill switch only logs an error.
- **tello**: the shipped config enables the camera and sets `camera.camera_name`, so as2_core reads
  `camera.encoding`, which isn't set. Startup likely throws. `eulerToQuaternion(pitch, roll, yaw)`
  swaps roll and pitch. `clampSpeed` does nothing. IMU angular velocity is always zero. The YAML keys
  `state_freq` and `camera.stream_url` are ignored by the code.
- **betaflight**: the kill switch loops forever and blocks the node. Yaw uses `roll_slope_`. `onRc`
  and `onMotor` write `dim[0]` after only `reserve(1)`, which is undefined behaviour. With
  `msp_override_channels_mask=15` the ARM and kill channels sent over MSP are probably ignored.

## Plugin settings in `config_file` are overwritten by the plugin defaults

`as2_state_estimator/launch/state_estimator_launch.py` passes the node `[{...}, config_file,
plugin_config_file]`, and `plugin_config_file` defaults to the plugin's `plugin_default.yaml`. The
later file wins, so any plugin key in `config_file` that also exists in the defaults is replaced
(checked with the node binary and two `--params-file`, not end to end through `ros2 launch`).
project_gazebo's `ground_truth.twist_sub_topic: "ground_truth/twist"` is probably reset to `""`.
The concepts pages and the simulator pages currently show plugin keys in `config.yaml`.

The trajectory generator launch is worse: it appends `plugins/<p>/config/<p>.yaml` after
`config_file` and has no `plugin_config_file` argument, so plugin settings can't be changed from a
launch file at all. The composable launches of the trajectory generator and the path planner don't
load the plugin YAML, so plugins run on their code defaults, which differ a lot.

Fix: load the plugin defaults first, then `config_file`, then `plugin_config_file`, and add
`plugin_config_file` to the trajectory generator launch.

Update 2026-11-03: two separate end-to-end runs through `ros2 launch` didn't reproduce this for the
state estimator. `ground_truth.twist_sub_topic` and the simple_ekf and mocap settings placed in
`config.yaml` under the node's name were applied. The docs callout was softened. Recheck with a
setting under a bare `/**: ros__parameters:` before fixing anything.

## Plugin bugs found while writing the plugin pages

- **pid_speed_controller**: `speed_in_a_plane_control.antiwindup_cte` calls `set_alpha` on the
  planar PID (pid_speed_controller.cpp:485). Position and trajectory with yaw speed never command a
  yaw rate. Default `alpha` 0.0 disables every Kd.
- **differential_flatness_controller**: `resetCommands()` zeroes the integral every cycle, so Ki
  does nothing. `antiwindup_cte / Ki` divides by zero when Ki is 0.
- **geometric_controller**: trajectory to body rates is refused when `velocity.kp` is zero, though
  that path doesn't use it.
- **takeoff**: `takeoff_height` is read but never used. Success compares the relative goal height
  with absolute z, so a take-off from z != 0 targets one height and checks another.
- **path planner**: `follow_path_succeeded_`, `navigation_aborted_` and `follow_path_rejected_`
  are set but never reset in `on_activate`, so after one goal the next reports the old result at
  once (path_planner_behavior.cpp:152, 233, 260-274). No replanning, no check for an empty grid
  before the TF lookup, and `cellToPoint` subtracts half a cell instead of adding it. Every waypoint
  gets `z = 1.0` (path_planner_behavior.cpp:119). Voronoi marker
  namespace is "a_star". `distance_remaining` is to the next waypoint.
- **map server**: `depth2occ_grid` and `point_cloud2occ_grid` aren't libraries, only launch presets,
  but are listed in plugins.xml. The depth preset hard-codes `drone0` in `output_frame`.
- **gripper**: `gripper_behavior_launch.py` reads `plugins.xml` at the share root (it's under
  `gripper_behavior/`) and reads `config_file` while declaring `plugin_config_file`. `two_fingers`
  opens on any execution end. Payload isn't in the colcon CI on either distro.
- **object perception**: `threshold` filters nothing since every ArUco score is 1.0. The behavior
  never succeeds on its own, even with `persistent: false`.

## CLI bugs found while writing the CLI pages

- `as2 cd` checks `projects` with a substring match against the string `['projects']`
  (`as2_cli/bash_utils/as2_cd.bash`). Any part of that word, such as `as2 cd proj` or `as2 cd s`,
  sends the shell to a folder under `AEROSTACK2_PATH` that doesn't exist, and the cd fails.
- The `projects` and project-name checks in `as2_cd.bash` replace an exact package match, so a
  project folder shadows a package with the same name.
- `as2 cd -h` does nothing useful. It ignores the flag and goes to `AEROSTACK2_PATH`.
- `as2 -h` advertises `-y, --yes` and a `run` command. Neither exists, and `as2 clean` always asks.
- `as2 project` checks the connection with `ping google.com` before doing anything, even `-h`. It
  fails on networks that block ICMP although GitHub is reachable. A `curl` of the repos URL alone
  would be enough.
- `as2 project -v` must come before `-l`, since getopts runs `list_projects` as soon as it sees `-l`.
- `as2 clean <pkg>` removes the whole workspace `log` folder for every package cleaned.
- `as2 build`, `as2 clean` and `as2 test` echo stray debug output, such as `$SHELL` and each flag.

## User interface bugs found while writing the user interface pages

### Keyboard teleoperation (`as2_keyboard_teleoperation`)

- Space (hover) likely never fires. PySimpleGUI 4 reports keys with a character as that character,
  so space arrives as `' '`, but `KeyMappings.HOVER_KEY` is `'space'` (config_values.py).
  Delete may also arrive as `'\x7f'` on X11 instead of `Delete:<code>`. Test both in simulation.
- Unchecking "All" doesn't deselect. The handler sets every `drone_id[1] = True` either way
  (main_window.py:357-358), so commands still go to every drone.
- `r` "Reset orientation" is shown in the window but has no mapping or handler.
- The emergency kill switch publishes forever at 100 Hz until the process exits
  (drone_interface_base.py `send_emergency_killswitch_to_aircraft`).
- `pose_frame_id: base_link` stamps an absolute earth target with `<ns>/base_link`
  (drone_manager.py:206-216, 385-389).
- In Speed mode, the zero twist sent on each 50 ms timeout may switch the controller to speed mode
  while take-off or land runs.
- Settings apply on Exit and close too, invalid input silently becomes 0.0, and nothing persists.
- Reopening Settings or Localization calls `self.layout(...)` again on the built window
  (main_window.py:320, 331).
- Code fallbacks differ from the YAML: turn speed 0.30 vs 0.5, turn angle 1.57 vs 0.78.
- Every launch configuration in the context is forwarded as `--key=value`, and argparse rejects
  unknown ones, so including this launch file from a parent launch with its own arguments may fail.
- PySimpleGUI and python3-tk are not declared in package.xml or any pixi.toml. Only the humble
  Dockerfiles install it.
- Label typo: the `s` row in Speed mode reads "Set vertical speed" with no "to".

### Alphanumeric viewer (`as2_alphanumeric_viewer`)

- Platform tab Thrust never updates. The callback sets `actuator_command_thrust_aux`, the display
  checks `thrust_aux`, which is never set true (alphanumeric_viewer.cpp:246, 845).
- Platform tab actuator pose and twist use the self-localization flags `current_pose_aux` and
  `current_speed_aux` instead of the actuator command ones (cpp:875-905).
- Sensors "Altitude (-z)" prints +z. "Altitude (sea level)" and "Temperature" are never filled,
  and there's no temperature subscription.
- "DISARMED" is printed without padding, leaving residue such as "DISARMEDFF". Unknown states
  print nothing.
- Typo "Conected".
- Battery: NaN percentage prints blank, and the 0-1 vs 0-100 guess is sticky.
- Values from 100 up overflow their columns. No terminal size check or resize handling.
- Keys may not reach the node when started with the launch file. project_gazebo uses `ros2 run`.
- Platform and controller info never show as missing, they show defaults (DISARMED) until the first
  message. The header doc says missing values print dimmed, but they print dashes.

### Visualization (`as2_visualization`, `as2_rviz_plugins`)

- `paint_geozones:=true` is broken. The launch runs `geozones_marker_publisher`, which has no entry
  point in setup.py and no `main()`. An empty polygon would also raise IndexError (line 103).
- `as2_default.rviz` hardcodes `drone0`, so other namespaces and swarms show nothing by default.
- `swarm_viz.launch.py` splits `namespace_list` on `,` without stripping, while its own
  description example is `"drone0, drone1"`. It also doesn't forward `drone_model` or the paint flags.
- The velocity ARROW puts the velocity in `pose.position` with a zero quaternion and fixed scale
  (marker_publisher.py:138-151). The display is named "ReferenceVel" but shows measured twist.
- gate_publisher `--freq` is a period. It uses argparse, so `--ros-args` from a launch Node would be
  rejected.
- Adapter tooling (viz_launcher, cfg_generator): presets are named after their class, not their id,
  so adapters collide in VizBridge. Unknown `preset_type` returns None silently.
  `round(n/adapters_per_process)` can be 0 and crash. QoS lookup raises KeyError, not ValueError.
  VizBridge logs every message at INFO.
- Teleop panel: Land, Hover and Kill are enabled before a namespace is set, and their clients are
  null, so pressing them can crash RViz. Takeoff doesn't wait for arm and offboard, so it may need a
  second press. The panel nodes are never spun. No disarm after landing. A leading `/` in the
  namespace builds `//drone0/` for hover. The plugin description still says "diff-drive style robot
  base control".
- `std_srvs`, `rclcpp_action` and `pluginlib` are not declared in as2_rviz_plugins/package.xml.
  `pydantic` and `yaml` are not declared in as2_visualization.

## ROS 2 interface bugs found while writing the ROS 2 Interfaces pages

### Platform and sensors (`as2_core`)

- Service prefixes are inconsistent. `set_arming_state`, `set_offboard_mode`,
  `set_platform_control_mode`, `platform_takeoff` and `platform_land` sit at the namespace root,
  while `list_control_modes` and `state_machine_event` are under `platform/`.
- `platform/state_machine_event` is created with `generate_local_name`, so it follows the node name.
  Launch the platform under another name and the take-off and land behaviors, which use the header
  constant `platform/state_machine_event`, can't find it.
- `platform_takeoff` and `platform_land` only fire TOOK_OFF and LANDED. Called on their own they
  don't advance the state machine (TAKE_OFF or LAND must come first), but still return success.
- `sensor.cpp:89` keeps the camera's `StaticTransformBroadcaster` in a function-local `static`,
  so it is bound to the first node that calls it.
- `GroundTruth` builds its topics as `topic_name_base + "ground_truth/pose"` with no separator
  (sensor.cpp:385-386). A base like `drone0` gives `drone0ground_truth/pose`.
- `topics.hpp` declares `sensor_measurements::lidar` as `sensor_measurements/lidar`, but the map
  server hardcodes `sensor_measurements/lidar/scan` and Gazebo bridges `<lidar>/scan`.

### Motion controller

- In bypass, `sendCommand` copies `ref_pose_` and `ref_twist_` into the commands but never
  `ref_thrust_` (controller_handler.cpp:765-766). A bypassed attitude or body rates mode sends a
  zero thrust.
- `controller/list_control_modes` is in `services.hpp` and the Python bindings, but nothing serves it.
- `use_bypass` defaults to `false` in code (controller_handler.cpp:95) and `true` in the shipped yaml.
- `motion_reference::qos_trajectory` (reliable) exists, but the controller subscribes to
  `motion_reference/trajectory` with sensor data QoS.
- `platform/list_control_modes` and `set_platform_control_mode` clients block forever, retrying every
  second, inside the `set_control_mode` callback when the platform isn't up.

### State estimator

- `self_localization/pose` and `twist` are published with `rclcpp::QoS(10)` (reliable), while
  `topics.hpp` gives `self_localization::qos = SensorDataQoS()`. Subscribers written against the
  header still work, but the header is wrong.
- `self_localization/odom` is in the names header and bindings, but nothing publishes it.
- Every timer is a wall timer, so `publish_hz` and `debug_publish_hz` ignore `use_sim_time`.

### Behaviors

- `BehaviorServer` names the action from the constructor argument but the `_behavior/*` services and
  status topic from `get_name()` (behavior_server__impl.hpp:67 vs 114). The composable path planner
  launch names the node `PathPlannerBehavior`, so the action stays `path_planner` while the services
  move, and the Python API (which uses `path_planner` for both) can't pause, stop or modify it.
- `SetArmingStateBehavior::on_run` sets `result_msg->success = true` on failure
  (set_arming_state_behavior.hpp:117). The offboard behavior gets it right.
- `as2_behaviors_platform/launch/as2_platform_behaviors_launch.py:73-133` launches the packages
  `takeoff_behavior`, `land_behavior`, `go_to_behavior` and `follow_path_behavior`, which no longer
  exist. They live in `as2_behaviors_motion`.
- Only six behaviors have names in `actions.hpp`. The other nine hardcode literals, and
  `path_planner` is the only snake_case one.
- `modify` calls `on_modify` but doesn't replace the goal handle's goal, so `on_run` keeps receiving
  the original goal (behavior_server__impl.hpp:240-243 vs 290).
- Cancel and stop reset `goal_handle_` without calling `canceled()` or `abort()`. In practice the client still
  gets CANCELED with the result, probably when the reset handle is destroyed, so nothing hangs.
- The trajectory generator subscribes to `motion_reference/yaw` (hardcoded, not in the header), which
  nothing publishes.
- `PathPlannerBehavior` is registered as a component twice (path_planning CMakeLists.txt:73, 126).
- Object perception defaults to `sensor_measurements/camera/image_raw`, which fits Gazebo but not
  platforms using the sensor helpers (`<camera>/image`).

### Swarm flocking

- The per-drone pose subscription is relative, so with the launch's `Swarm` namespace it resolves
  to `/Swarm/drone0/self_localization/pose`, while the action clients are absolute
  (drone_swarm.cpp:48 vs 68-71).
- `modifySwarmSrv` checks `find == end` and then calls `.at()`, which throws for new drones, and
  never sets `response->accepted = true` (swarm_flocking_behavior.cpp:100-110).
- The constructor ignores `NodeOptions`, and `ModifySwarm.srv` is included but unused.

### Utilities

- No node serves `geopath_to_path`. The BT GoToGps and GpsToCartesian nodes wait for it forever.
- Geozones republish the alert on every check while the condition holds. The example config uses
  `alert: 1` (FORCE_HOVER), which nothing acts on.
- Geozones `set_geozone` with `data_type: gps` stores lat/lon unconverted, then `get_geozone` runs
  Local2LatLon on it. If no YAML zone was gps, `gps_handler` is null and gets dereferenced. RViz
  polygons are stamped `earth` but hold map-local points.
- external_object_to_tf launches at `/` by default, so `get_origin` resolves to `/get_origin`.
  A failed `setupGPS` leaves `gps_handler` null, later dereferenced. The `bank` field of
  AddStaticTransformGps is ignored, and `azimuthToQuaternion` is dead code with w and z swapped.
- Python `DroneInterfaceBase.__send_emergency` publishes in a `while rclpy.ok()` loop and never
  returns.
- The Python motion_ref_handler module creates a `traj_gen/run_node` client with no server, so every
  DroneInterface using it waits 3 s and warns.
- The mission executor logs REPEAT, INSERT, MODIFY, REMOVE and RESET as unimplemented.
- `as2_names.topics.global` is unusable in Python, since `global` is a keyword.

### as2_msgs

- Unused types: FollowTargetInfo, MissionEvent, NodeStatus, Speed, TrajGenInfo, DynamicFollower,
  DynamicLand, PackagePickUp, PackageUnPick, PathToGeopath, SetSpeed, ModifySwarm,
  DetectArucoMarkers. Unused names in the headers: `traj_gen/*` services, `traj_gen_info`,
  `follow_target/*`, `behavior/*`, `set_speed`, `gps::path_to_geopath` and `geopath_to_path` (both `""`).
- `YawMode.YAW_FROM_ORIENTATION` is handled nowhere.
- `DetectArucoMarkers` feedback field is spelled `sucess`.
- Copy-pasted header comments: GeopathToPath and PathToGeopath (wrong name, wrong direction),
  AddStaticTransformGps, SetControlMode, Get/SetGeozone, ModifySwarm, TrajectorySetpoints,
  GoToWaypoint result ("failed to takeoff"), FollowReference `max_speed_y`/`z` ("in x").
- `Acro.msg` keeps the old name after ACRO became BODY_RATES. `TrajectoryPoint.twist` is a Vector3
  velocity. package.xml says 1.1.3 while CMake says 1.0.0.

### Parameter estimation and swarm (found while merging the Robot Behaviors section)

- `mass_estimation_behavior/config/config_default.yaml` sets `minimum_mass: 0.6` and
  `maximum_mass: 0.6`, so every estimate is clamped to 0.6 kg.
- Force estimation returns `std::abs(u_thrust - mass * a_z)` (force_estimation.cpp:54), so it loses
  the sign and can't tell extra load from extra lift.
- The old docs say swarm flocking follows Reynolds' boids rules (separation, cohesion, alignment).
  The code has none: it places static TFs for the formation and sends each drone a FollowReference
  goal at its TF, with fixed 15 m/s speed limits (drone_swarm.cpp:97-106).

## Contributing setup (found while writing the issue and pull request pages)

- `CONTRIBUTING.md` links to https://aerostack2.github.io/contributing, which is a 404. The new pages
  are at aerostack2.dev/docs/develop/issues and /pull-requests.
- The bug report template only offers ROS 2 `humble` or `other`, and Aerostack2 versions v1.0.5 to
  v1.0.9 or main. No Jazzy, no 1.1.x, no 1.2.0.
- `.github/PULL_REQUEST_TEMPLATE/PULL_REQUEST_TEMPLATE.yml` is a YAML form, which GitHub doesn't
  support for pull requests, so it's dead. It also asks for `[as2_pkg] <title>` titles and offers
  galactic, while CONTRIBUTING.md asks for Conventional Commits titles.
- The markdown PR template lists only Humble as a tested ROS 2 version.
- PR titles aren't checked in CI. commitlint only runs in the local hook, and squash merges use the
  PR title when there are several commits.
- Issue titles use `[as2_pkg] <title>` while PR titles use Conventional Commits. Two conventions.
- `as2_behaviors_param_estimation` and `as2_behaviors_payload` aren't in the `package-name` lists of
  build-humble.yaml or build-jazzy.yaml, so CI never builds or tests them.
- `aerostack2/as2_node_template` (linked from the old New package page) was last pushed 2024-06 and
  predates the per-package pixi.toml.

## Python API and packaging bugs found while writing the tutorials

Found on 2026-11-03 by running the tutorial scripts against the multirotor simulator, with Pixi on
macOS and packages built from main.

- `drone.follow_path(path, speed)` crashes when `yaw_angle` isn't passed. `FollowPathModule.__call__`
  defaults `yaw_angle` to `None`, and `FollowPathBehavior.start()` assigns it straight to
  `YawMode.angle`, which raises `AssertionError: The 'angle' field must be of type 'float'`
  (followpath_behavior.py:72). The helpers `follow_path_with_keep_yaw`, `follow_path_with_yaw` and
  `follow_path_with_path_facing` pass `0.0` and work. `go_to` handles `None`. Fix: default to
  `0.0`, or convert `None` to `0.0` in `start()`.
- `as2_motion_reference_handlers` imports scipy (`utils.py:42`, `scipy.spatial.transform.Rotation`)
  but its package.xml doesn't declare it. Anything that loads the Python motion reference handlers,
  like the keyboard teleoperation through `DroneInterfaceTeleop`, fails with
  `ModuleNotFoundError: No module named 'scipy'` on an install that doesn't happen to have it. Seen
  with the Pixi packages, and rosdep won't install it on apt either. Fix: add
  `<exec_depend>python3-scipy</exec_depend>`.
- `drone.takeoff(height=1.5, speed=0.5)` returned `True` with `drone.position` at z = 1.23 to 1.29.
  Explained below under "The Python API reads its topics 20 messages a second": the position lags.

## Bugs found while writing the tutorials and Develop guides

Found on 2026-11-03 by building small plugins, a platform, a behavior and missions with Pixi on
macOS, Humble, with packages built from main, and flying them in the multirotor simulator or a
headless Gazebo Fortress.

### Plugins from your own package can't be loaded by the stock launch files

- `as2_motion_controller/launch/controller_launch.py:172`, `as2_state_estimator/launch/state_estimator_launch.py:152`,
  `follow_reference_behavior_launch.py:71` and `generate_polynomial_trajectory_behavior_launch.py:80,133`
  build `choices` for `plugin_name` from the package's own `plugins.xml`, so any plugin from another
  package is rejected. Other plugin launch files probably follow the same pattern.
- The controller and estimator launches also open the default plugin config at
  `share/<pkg>/plugins/<name>/config/...` unconditionally (controller_launch.py:100,111;
  state_estimator_launch.py:95,101, via `launch_configuration_from_config_file.py:175`), which raises
  FileNotFoundError for an outside plugin even when `plugin_config_file` is passed.
- Their fallback that picks `plugin_name` from `config_file` (controller_launch.py:82,
  state_estimator_launch.py:75) keeps the last `plugin_name` in the file when none is a stock plugin,
  which can belong to another node such as TrajectoryGeneratorBehavior.
- `plugin_name:=simple_ekf` on the command line loses to `plugin_name` in `config_file` for the state
  estimator. The node then loads ground_truth with the simple_ekf defaults and crashes with
  "Statically typed parameter 'ground_truth.pose_sub_topic' must be initialized".

Fix: list plugins from the pluginlib ament index (`<pkg>__pluginlib__plugin`) or drop `choices`,
resolve default configs from the package that declares the plugin and skip them when missing, and
let the launch argument win over `config_file`. The guides start the nodes with `ros2 run` and
`--params-file` meanwhile.

### Plugin and behavior bases

- **Estimator plugins can't report fixed transforms in `onSetup`.** `plugin_wrapper.cpp:96` calls
  `setup()` inside `PluginWrapper::create`, but `as2_state_estimator.cpp:109` only registers the
  plugin after create returns. The report is refused with "The plugin X is not authorized to publish
  in the earth_to_map frame" and the pose is never published. Fix: register before `setup()`.
- **The trajectory generation package doesn't export its plugin base.**
  `generate_polynomial_trajectory_behavior/CMakeLists.txt:76-78` calls `ament_export_*` in a
  subdirectory, so they never reach the root `ament_package()`, and an outside plugin can't
  `find_package` and link the base. Fix: move the exports to the root CMakeLists.
- `follow_reference_behavior.cpp:69` calls `this->~FollowReferenceBehavior()` in the constructor
  when the plugin fails to load. Undefined behaviour. Read only.

### Motion controller

- **No hover when no mode pair fits.** `controller_handler.cpp:455-458` returns failure right away
  when no pair fits the request. The hover fallback only runs when pairs exist but the plugin refuses
  them all. With the differential flatness controller and bypass off, a speed request was refused,
  the drone didn't hover and ended on the ground. Retries of `set_control_mode` took 1.3 s up to
  2.6 s, `hover()` then timed out and a failed land left the platform in EMERGENCY.
- **`hover()` waits for a mode the controller never reports.** `basic_motion_references.py:357-363`
  waits for `controller/info` to say HOVER, but with bypass off a hover held by the plugin is reported
  as POSITION. It logs "Timeout waiting for mode 1" and returns False after 5 s while the drone
  hovers. Fix: accept POSITION after a HOVER request.

### Aerial platform

- **The kill switch leaves `armed` and `offboard` true.** `aerial_platform.cpp:349` never clears
  `platform_info_msg_.armed`, so `platform/info` keeps reporting armed and offboard in EMERGENCY.
- **Multirotor simulator force noise is never set.** `as2_platform_multirotor_simulator.cpp:547-550`
  assigns `force_process_noise` to `moment_process_noise_auto_correlation`, which is then overwritten.

### Python API

- **The interface reads its topics at most 20 messages a second.** `drone_interface_base.py:265-268`
  `__auto_spin` handles one callback with `spin_once(timeout_sec=0)` and then sleeps 1/spin_rate, so
  pose, twist, info and action traffic share 20 callbacks a second. `drone.position` lagged 0.1 to
  0.3 m at 0.5 m/s and repeated stale values. Fix: spin the executor in the thread, or
  `spin_once(timeout_sec=interval)` in a loop.
- **`navigate_to` returns `None`.** `navigate_to_module.py:71,80` drop the result of `self.start()`,
  and the helpers have no `wait` argument. Fix: return it from every caller.
- **`AS2_MODULES_PATH` is never set under Pixi**, only by as2_python_api's colcon hook. When unset,
  `tools/utils.py:122` `os.getenv('AS2_MODULES_PATH').split(':')` raises AttributeError for any
  custom or misspelled module. Fix: default to `''` and ship a conda activation script.
- `tools/utils.py:102` prints `spec: ModuleSpec(...)` on every `load_module`.
- `tools/utils.py:37` imports `importlib` but uses `importlib.util`.
- `module_base.py:63` `__del__` raises "Exception ignored ... no attribute `_ModuleBase__drone`"
  when the module's `__init__` failed.

### Path planner

- The second-goal bug above, made precise: after one goal succeeds, the next is accepted and a new
  FollowPath is sent, but the behavior reports SUCCESS within about 1.4 s while the drone keeps flying
  for about 40 s. After an aborted goal, the next fails at once but still sends the FollowPath. Fix:
  reset the three flags and `follow_path_feedback_` in `on_activate`.

### Gazebo assets

- **Template errors are swallowed.** `models/drone.py:313` only raises on "undefined local". A
  `TemplateNotFound` is printed and the launch spawns whatever stale file is at `/tmp/<type>_<i>.sdf`.
- **Rendered SDFs go to fixed paths.** `drone.py:275` and `world.py:152` write `/tmp/<world>.sdf` and
  `/tmp/<model>_<i>.sdf`, so two simulations at once overwrite each other. Fix: a temp dir.
- **Drone templates outside the package can't include stock sensor templates.** The Jinja root is
  the template's own models folder (`drone.py:236`). Fix: search the package models and every folder
  on `GZ_SIM_RESOURCE_PATH`.
- No way to add a payload type from outside, like `AS2_EXTRA_DRONE_MODELS` does for drones.
- `azimuth_bridge.cpp:77` subscribes to `/model/gps/odometry` hardcoded, so it only works for an
  object named gps.
- `payload.py:119` lists `d435`, which has no model. `point_lidar` is marked FIXME not working.
  The docs table no longer lists either.
- `hooks/resource_paths.sh:3-4` still uses the old name `as2_ign_gazebo_assets`. Harmless.
- Headless Gazebo Fortress on macOS crashes in ogre2 as soon as a rendering sensor spawns ("NSWindow
  should only be instantiated on the main thread"), so cameras and lidars can't be simulated on macOS
  with Pixi and Humble.

### Packaging

- as2_behaviors_trajectory_generation's bundled libmav_trajectory_generation needs libglog.1.dylib,
  but glog isn't a run dependency. The trajectory generator crashed at start until `glog<0.7` was
  added. Seen with the local Pixi channel.

### Example projects

- **project_as2_multirotor_simulator:** `launch_as2.bash` uses `getopts "cmn:g"`, so `-c` takes no
  value and `-c df` fails with "Invalid motion controller plugin". Fix: `getopts "c:mn:g"`.
  `config/differential_flatness_controller.yaml` sets an unknown `trajectory_control.alpha`. Its Pixi
  support is only on the `feat/pixi` branch, which the Quickstart and tutorials rely on.
- **project_gazebo:** the GUI pane runs `gz sim -g` whenever `GZ_VERSION` is set, but on Humble it's
  `fortress`, which has no `gz` binary. Fix: compare against `harmonic`. `mission.py` declares
  `--use_sim_time` as `store_true` with `default=True`. The RViz Camera display points at a topic that
  doesn't exist. `stop.bash` runs `pkill -9 -f 'gz'`, which kills anything with "gz" in its command.
- **project_dji_tello:** `config.yaml` uses `plugin_name: "mocap_pose"`, which no longer exists on
  main.
- **project_crazyflie:** the commented per-drone mocap override uses `state_estimator:
  rigid_body_name` instead of `ground_truth.rigid_body_name`, and its ground station usage text
  describes `-m` backwards.

## Decisions pending for the docs

Open questions from writing the tutorials and Develop guides on 2026-11-03. Each lists the options
and what the pages do now.

### Waiting on upstream fixes

These pages carry workarounds or warnings that should go once the bug above is fixed.

| Page | Workaround now | Remove when |
| --- | --- | --- |
| Tutorials, Fly by hand | Installs scipy with the GUI library | as2_motion_reference_handlers declares scipy |
| Tutorials, A mission in Python | Uses `follow_path_with_path_facing` instead of `follow_path(...)` | `follow_path` accepts no `yaw_angle` |
| Tutorials, Planning around obstacles | Warns to restart the planner before each goal, and uses `wait=False` plus `wait_to_result()` | The planner resets its flags, and `navigate_to` returns its result |
| Tutorials, The motion controller | Warn callout with the `getopts "c:mn:g"` fix for `-c df` | project_as2_multirotor_simulator is fixed |
| Develop, controller, estimator, follow reference and trajectory generation guides | Start the node with `ros2 run` and `--params-file`, with a warn callout | The launch files accept plugins from other packages |
| Develop, Estimator plugin | Callout that `onSetup` can't report transforms | The wrapper registers the plugin before `setup()` |
| Develop, Trajectory generation plugin | Finds the base header and library in the install prefix by hand | The package exports its base |
| Develop, Behavior | Sets `AS2_MODULES_PATH` with an activation env in Pixi | as2_python_api sets it under Pixi |

Every Pixi tab in the tutorials also relies on merging project_as2_multirotor_simulator's
`feat/pixi` branch.

### Content choices

- **Fake lidar in Planning around obstacles.** The page includes a 60-line `fake_lidar.py` because
  the multirotor simulator has no sensors. The alternative is a hand-made occupancy grid, which works
  but skips the map server. Now uses the fake lidar.
- **Tello project.** project_dji_tello's `config.yaml` uses the removed `mocap_pose` plugin, and the
  real-drone tutorial tells readers to replace it with the `ground_truth` block. Fix the project
  upstream and drop that step, or point the page at another project.
- **Crazyflie naming.** The Crazyflie platform page uses `/cf0` with a separate `swarm.yaml`, while
  project_crazyflie and the real-drone tutorial use `drone0` in one `config.yaml`. Pick one.
- **Tello speed limit.** The Tello platform page says speeds are limited to 1 m/s, but `clampSpeed`
  does nothing (see above). Fix the code or the page. The tutorial makes no claim.
- **Cameras on macOS.** Headless Gazebo Fortress crashes on rendering sensors, so the camera steps of
  Move to Gazebo and the camera part of Gazebo assets were never run. Harmonic with Jazzy is untested
  on macOS. Decide whether to say macOS has no simulated cameras.
- **Gazebo assets, part two.** The contributor part adds a camera type by editing as2_gazebo_assets.
  Keep it on that page or move it, and part one uses a GPS for the drone variant because cameras
  can't render here and stock sensor templates can't be included from a project.
- **Behavior trees for a new behavior.** The Behavior guide explains that a tree node needs its own
  runner and links `BtActionNode`, without a full example.
- **Step response numbers.** The motion controller tutorial's table is one run per gain, and runs
  vary about 10 to 15 %.
- **Plugin settings placement.** The state estimation tutorial and the plugins page now say plugin
  settings can go in `config_file` under the node's name. Recheck the bare `/**` case before
  documenting either way.

### Style choices in the guides

- **Build tabs.** The Develop guides label their tabs "colcon" and "Pixi", while the tutorials use
  "apt, source or Docker" and "Pixi". Keep the difference or align them.
- **Code layout.** Each guide keeps its plugin in a single `.cpp` without a license header and says
  the stock plugins split header and source. Packages have no `as2_` prefix.
- **Plugin names.** `follow_reference_plugin_speed` follows Aerostack2's naming, so a future stock
  plugin could clash with it. Rename it, for example to `my_follow_reference_plugin`.
- **Action location.** The Behavior guide keeps `Circle.action` in the behavior's package, with a note
  that a shared interfaces package is better when others need it.
