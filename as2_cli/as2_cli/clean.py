import os
import shutil

import click

from as2_cli.env import get_aerostack2_path, get_aerostack2_workspace
from as2_cli.packages import find_packages


@click.command()
@click.argument("packages", nargs=-1)
@click.option("-a", "--all", "clean_all", is_flag=True, help="Clean entire workspace.")
@click.option("-y", "--yes", is_flag=True, help="Skip confirmation prompt.")
def clean(packages, clean_all, yes):
    """Clean workspace build artifacts."""
    workspace = get_aerostack2_workspace()

    if clean_all:
        if not yes:
            click.confirm("Are you sure you want to clean the entire workspace?", abort=True)
        click.echo("Cleaning all")
        for d in ("build", "install", "log"):
            target = os.path.join(workspace, d)
            if os.path.isdir(target):
                shutil.rmtree(target)
        return

    if not packages:
        raise click.ClickException("No package specified. Use -a/--all to clean everything.")

    as2_path = get_aerostack2_path()
    known = {name for name, _ in find_packages(as2_path)}

    for pkg in packages:
        if pkg not in known:
            raise click.ClickException(f"Package '{pkg}' not found")

    if not yes:
        click.confirm(f"Are you sure you want to clean {' '.join(packages)}?", abort=True)

    click.echo(f"Cleaning {' '.join(packages)}")
    for pkg in packages:
        for d in ("build", "install"):
            target = os.path.join(workspace, d, pkg)
            if os.path.isdir(target):
                shutil.rmtree(target)
    log_dir = os.path.join(workspace, "log")
    if os.path.isdir(log_dir):
        shutil.rmtree(log_dir)

    click.echo("Done")
