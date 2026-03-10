from click.testing import CliRunner

from as2_cli.cli import cli


def test_project_list(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["project", "-l"])
    assert result.exit_code == 0
    assert "gazebo" in result.output
    assert "crazyflie" in result.output
    assert "px4_vision" in result.output
    assert "dji_osdk" in result.output


def test_project_list_verbose(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["project", "-l", "-v"])
    assert result.exit_code == 0
    assert "github.com" in result.output


def test_project_no_option(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["project"])
    assert result.exit_code != 0
    assert "No option specified" in result.output
