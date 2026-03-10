import subprocess
import sys

import click

from as2_cli.env import get_aerostack2_workspace, get_ros_distro


@click.command()
@click.argument("package", default="")
@click.option("-v", "--verbose", is_flag=True, help="Test with verbose output.")
def test(package, verbose):
    """Run tests with colcon."""
    ros_distro = get_ros_distro()
    workspace = get_aerostack2_workspace()

    verbose_flag = "--event-handlers console_direct+" if verbose else ""
    pkg_flag = f"--packages-select {package}" if package else ""

    cmd = f"source /opt/ros/{ros_distro}/setup.bash && cd {workspace} && colcon test {pkg_flag} {verbose_flag}"

    if not verbose:
        test_result_base = f"--test-result-base ./build/{package}" if package else ""
        cmd += f" && colcon test-result --verbose {test_result_base}"

    result = subprocess.run(["bash", "-c", cmd])
    sys.exit(result.returncode)
