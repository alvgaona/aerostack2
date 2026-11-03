Title: Use one numbering for control modes in messages and platform configs
Labels: enhancement, msgs, platform, core

## Summary

Control modes have two different numberings. The `ControlMode` message numbers them one way, and the
platform `control_modes.yaml` files use a bit code that numbers them differently. Position is 2 in
the message and 6 in the config files, and body rates is 6 in the message and 2 in the config files.

Nothing breaks when the drone runs, because the code converts between the two. But anyone writing a
platform or reading a config by hand has to keep both tables in mind, and a mix-up enables the wrong
control mode on a platform.

## Where

The message numbering:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_msgs/msg/ControlMode.msg#L11-L18

The config file numbering, for example in the multirotor simulator:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_aerial_platforms/as2_platform_multirotor_simulator/config/control_modes.yaml#L5-L14

The conversion between them:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_core/src/utils/control_mode_utils.cpp#L185-L210

## Acceptance criteria

- [ ] A control mode has the same number, or the same name, in the message and in the platform
      config files.
- [ ] A config file can be read without a lookup table or bit arithmetic.
- [ ] Config files from the stock and external platforms keep working, or are rejected with a
      message that says how to update them.
- [ ] The platform docs describe the config format, including the yaw mode `none`, which the code
      supports but the config comments leave out.
