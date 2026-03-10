import os
import subprocess

import click

from as2_cli.env import get_aerostack2_projects

PROJECTS = [
    {
        "name": "gazebo",
        "url": "https://github.com/aerostack2/project_gazebo.git",
        "version": "main",
    },
    {
        "name": "crazyflie",
        "url": "https://github.com/aerostack2/project_crazyflie.git",
        "version": "main",
    },
    {
        "name": "px4_vision",
        "url": "https://github.com/aerostack2/project_px4_vision.git",
        "version": "main",
    },
    {
        "name": "dji_osdk",
        "url": "https://github.com/aerostack2/project_dji_osdk.git",
        "version": "main",
    },
]


def _list_projects(verbose):
    click.echo(
        "\n" + click.style("[*]", fg="yellow") +
        click.style(" Listing projects...\n", fg="white")
    )
    for i, proj in enumerate(PROJECTS, start=1):
        idx = click.style(str(i), fg="blue")
        ver = click.style(f"[{proj['version']}]", fg="yellow")
        if verbose:
            click.echo(f"{idx}: {proj['name']} {ver} -> {proj['url']}")
        else:
            click.echo(f"{idx}: {proj['name']} {ver}")
    click.echo()


def _install_project(proj, projects_path):
    dest = os.path.join(projects_path, proj["name"])
    if os.path.isdir(dest):
        click.echo(click.style(f"[!] Project {proj['name']} already exists.", fg="red"))
        return

    click.echo(
        "\n" + click.style("[*]", fg="yellow") +
        f" Installing project {proj['name']} ...\n"
    )
    cmd = ["git", "clone", "--branch", proj["version"], proj["url"], dest]
    click.echo(" ".join(cmd))
    subprocess.run(cmd)


@click.command()
@click.option("-l", "--list", "list_projects", is_flag=True, help="List available projects.")
@click.option("-i", "--install-id", help="Install projects by comma-separated IDs.")
@click.option("-n", "--install-name", help="Install projects by comma-separated names.")
@click.option("-v", "--verbose", is_flag=True, help="Verbose output.")
def project(list_projects, install_id, install_name, verbose):
    """List and install Aerostack2 projects."""
    projects_path = get_aerostack2_projects()

    if not any([list_projects, install_id, install_name]):
        raise click.ClickException("No option specified. Use -l, -i, or -n.")

    if list_projects:
        _list_projects(verbose)

    if install_id:
        click.echo(
            "\n" + click.style("[*]", fg="yellow") +
            click.style(" Installing projects by id...\n", fg="white")
        )
        for id_str in install_id.split(","):
            idx = int(id_str.strip())
            if idx < 1 or idx > len(PROJECTS):
                click.echo(click.style(f"[!] Project id {idx} is not available.", fg="red"))
                continue
            _install_project(PROJECTS[idx - 1], projects_path)

    if install_name:
        click.echo(
            "\n" + click.style("[*]", fg="yellow") +
            click.style(" Installing projects by name...\n", fg="white")
        )
        proj_map = {p["name"]: p for p in PROJECTS}
        for name in install_name.split(","):
            name = name.strip()
            if name not in proj_map:
                click.echo(click.style(f"[!] Project {name} is not available.", fg="red"))
                continue
            _install_project(proj_map[name], projects_path)
