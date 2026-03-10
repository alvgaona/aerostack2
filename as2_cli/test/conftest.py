import os
import pytest


@pytest.fixture
def fake_aerostack(tmp_path):
    as2_path = tmp_path / "src" / "aerostack2"
    as2_path.mkdir(parents=True)

    for pkg_name in ("as2_core", "as2_state_estimator", "as2_cli"):
        pkg_dir = as2_path / pkg_name
        pkg_dir.mkdir()
        (pkg_dir / "package.xml").write_text(
            f'<?xml version="1.0"?>\n'
            f"<package format=\"3\">\n"
            f"  <name>{pkg_name}</name>\n"
            f"  <export><build_type>ament_cmake</build_type></export>\n"
            f"</package>\n"
        )

    projects_dir = as2_path / "projects"
    projects_dir.mkdir()
    for proj in ("gazebo", "crazyflie"):
        (projects_dir / proj).mkdir()

    return as2_path


@pytest.fixture
def env_vars(fake_aerostack, monkeypatch):
    monkeypatch.setenv("AEROSTACK2_PATH", str(fake_aerostack))
    monkeypatch.setenv("AEROSTACK2_WORKSPACE", str(fake_aerostack.parent.parent))
    monkeypatch.setenv("AEROSTACK2_PROJECTS", str(fake_aerostack / "projects"))
    return fake_aerostack
