import subprocess
import sys

import click

from as2_cli.env import get_aerostack2_workspace, get_ros_distro


@click.command()
@click.argument("package", default="")
@click.option("-d", "--debug", is_flag=True, help="Build in debug mode.")
@click.option("-v", "--verbose", is_flag=True, help="Build with verbose output.")
def build(package, debug, verbose):
    """Build packages with colcon."""
    ros_distro = get_ros_distro()
    workspace = get_aerostack2_workspace()
    build_type = "Debug" if debug else "Release"

    verbose_flag = "--event-handlers console_direct+" if verbose else ""
    pkg_flag = f"--packages-up-to {package}" if package else ""

    setup_cmd = f"source /opt/ros/{ros_distro}/setup.bash"
    if package:
        setup_cmd += f" && source {workspace}/install/setup.bash"

    cmd = (
        f"{setup_cmd} && cd {workspace} && "
        f"colcon build --symlink-install {pkg_flag} {verbose_flag} "
        f"--cmake-args -DCMAKE_BUILD_TYPE={build_type}"
    )

    result = subprocess.run(["bash", "-c", cmd])
    sys.exit(result.returncode)
