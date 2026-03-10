import os

import click


def require_env(name):
    value = os.environ.get(name)
    if not value:
        raise click.ClickException(f"{name} environment variable is not set")
    return value


def get_aerostack2_path():
    return require_env("AEROSTACK2_PATH")


def get_aerostack2_workspace():
    return require_env("AEROSTACK2_WORKSPACE")


def get_aerostack2_projects():
    return require_env("AEROSTACK2_PROJECTS")


def get_ros_distro():
    return require_env("ROS_DISTRO")
