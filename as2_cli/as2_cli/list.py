import click

from as2_cli.env import get_aerostack2_path, get_aerostack2_projects
from as2_cli.packages import find_packages, find_projects


@click.command("list")
@click.option("-v", "--verbose", is_flag=True, help="Show package paths.")
@click.option("-p", "--plain", is_flag=True, help="Disable colored output.")
@click.option("--list-format", is_flag=True, help="Machine-readable output.")
@click.option("--projects", "show_projects", is_flag=True, help="List projects instead of packages.")
def list_cmd(verbose, plain, list_format, show_projects):
    """List packages or projects."""
    if list_format:
        plain = True

    if show_projects:
        projects_path = get_aerostack2_projects()
        items = find_projects(projects_path)
    else:
        as2_path = get_aerostack2_path()
        items = find_packages(as2_path)

    if list_format:
        if verbose:
            click.echo(" ".join(f"{name} {path}" for name, path in items))
        else:
            click.echo(" ".join(name for name, _ in items))
        return

    label = "projects" if show_projects else "packages"
    if plain:
        click.echo(f"\n[*] List of {label}:\n")
    else:
        click.echo(
            "\n" + click.style("[*]", fg="yellow") +
            click.style(f" List of {label}:\n", fg="green")
        )

    for i, (name, path) in enumerate(items):
        idx = click.style(f"[{i}]", fg="green") if not plain else f"[{i}]"
        name_styled = click.style(name, fg="white") if not plain else name
        if verbose:
            click.echo(f"{idx} {name_styled} -> {path}")
        else:
            click.echo(f"{idx} {name_styled}")
