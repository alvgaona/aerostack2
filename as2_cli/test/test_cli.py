from click.testing import CliRunner

from as2_cli.cli import cli


def test_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["--help"])
    assert result.exit_code == 0
    assert "Aerostack2 command-line tool." in result.output
    assert "switch" in result.output
    assert "build" in result.output


def test_switch_no_arg(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["switch"])
    assert result.exit_code == 0
    assert str(env_vars) in result.output


def test_switch_exact_package(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["switch", "as2_core"])
    assert result.exit_code == 0
    assert result.output.strip().endswith("as2_core")


def test_switch_prefix_fallback(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["switch", "core"])
    assert result.exit_code == 0
    assert result.output.strip().endswith("as2_core")


def test_switch_project(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["switch", "gazebo"])
    assert result.exit_code == 0
    assert result.output.strip().endswith("gazebo")


def test_switch_not_found(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["switch", "nonexistent"])
    assert result.exit_code != 0
    assert "not found" in result.output
