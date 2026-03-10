#!/bin/bash

if [ -z "$AEROSTACK2_PATH" ]; then
    echo "AEROSTACK2_PATH env var is unset. Please set it to the path of the AEROSTACK2_PATH folder"
else
    export AEROSTACK2_WORKSPACE=$(dirname $(dirname ${AEROSTACK2_PATH}))
    export AEROSTACK2_PROJECTS="$AEROSTACK2_PATH/projects/"

    ENV_VARIABLES_FILE="$AEROSTACK2_PATH/as2_cli/env_variables.bash"
    if test -f "$ENV_VARIABLES_FILE"; then
        source $ENV_VARIABLES_FILE
    else
        echo "export AEROSTACK2_SIMULATION_DRONE_ID=drone_sim_${USER}_0" >> $ENV_VARIABLES_FILE
        source $ENV_VARIABLES_FILE
    fi

    if [[ -f "$AEROSTACK2_WORKSPACE/install/setup.bash" && ! -z "$ROS_DISTRO" ]]; then
        source $AEROSTACK2_WORKSPACE/install/setup.bash
    fi

    as2() {
        if [ "$1" = "switch" ]; then
            shift
            local target
            target=$(command as2 switch "$@")
            [ $? -eq 0 ] && cd "$target"
        else
            command as2 "$@"
        fi
    }

    if [ "$ZSH_VERSION" = "" ]; then
        eval "$(_AS2_COMPLETE=bash_source as2)"
    else
        eval "$(_AS2_COMPLETE=zsh_source as2)"
    fi
fi
