from click.testing import CliRunner

from as2_cli.cli import cli


def test_list_packages(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["list"])
    assert result.exit_code == 0
    assert "as2_core" in result.output
    assert "as2_state_estimator" in result.output


def test_list_verbose(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["list", "-v"])
    assert result.exit_code == 0
    assert "->" in result.output


def test_list_plain(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["list", "-p"])
    assert result.exit_code == 0
    assert "\033" not in result.output


def test_list_format(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["list", "--list-format"])
    assert result.exit_code == 0
    output = result.output.strip()
    assert "as2_cli" in output
    assert "as2_core" in output
    assert "[" not in output


def test_list_format_verbose(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["list", "--list-format", "-v"])
    assert result.exit_code == 0
    output = result.output.strip()
    assert "as2_core" in output
    assert "/" in output


def test_list_projects(env_vars):
    runner = CliRunner()
    result = runner.invoke(cli, ["list", "--projects"])
    assert result.exit_code == 0
    assert "gazebo" in result.output
    assert "crazyflie" in result.output
