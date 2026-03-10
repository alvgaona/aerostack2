import click

from as2_cli.env import get_aerostack2_path
from as2_cli.packages import resolve_package_path


@click.group()
def cli():
    """Aerostack2 command-line tool."""


@cli.command()
@click.argument("package", default="")
def switch(package):
    """Change directory to a package or project path."""
    as2_path = get_aerostack2_path()
    result = resolve_package_path(package, as2_path)
    if result is None:
        raise click.ClickException(f"package '{package}' not found")
    click.echo(result)


from as2_cli.build import build  # noqa: E402
from as2_cli.test import test  # noqa: E402
from as2_cli.list import list_cmd  # noqa: E402
from as2_cli.clean import clean  # noqa: E402
from as2_cli.project import project  # noqa: E402

cli.add_command(build)
cli.add_command(test)
cli.add_command(list_cmd, name="list")
cli.add_command(clean)
cli.add_command(project)


def main():
    cli()
