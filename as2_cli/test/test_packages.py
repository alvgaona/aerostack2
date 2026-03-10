from as2_cli.packages import find_packages, find_projects, resolve_package_path


def test_find_packages(fake_aerostack):
    pkgs = find_packages(str(fake_aerostack))
    names = [name for name, _ in pkgs]
    assert "as2_core" in names
    assert "as2_state_estimator" in names
    assert "as2_cli" in names


def test_find_packages_sorted(fake_aerostack):
    pkgs = find_packages(str(fake_aerostack))
    names = [name for name, _ in pkgs]
    assert names == sorted(names)


def test_find_packages_returns_paths(fake_aerostack):
    pkgs = find_packages(str(fake_aerostack))
    pkg_map = dict(pkgs)
    assert pkg_map["as2_core"].endswith("as2_core")


def test_find_packages_ignores_non_ament(tmp_path):
    pkg_dir = tmp_path / "not_ament"
    pkg_dir.mkdir()
    (pkg_dir / "package.xml").write_text(
        '<?xml version="1.0"?>\n<package format="3">\n'
        "  <name>not_ament</name>\n</package>\n"
    )
    pkgs = find_packages(str(tmp_path))
    assert len(pkgs) == 0


def test_find_projects(fake_aerostack):
    projects_path = str(fake_aerostack / "projects")
    projects = find_projects(projects_path)
    names = [name for name, _ in projects]
    assert "gazebo" in names
    assert "crazyflie" in names


def test_find_projects_empty_dir(tmp_path):
    empty = tmp_path / "empty"
    empty.mkdir()
    assert find_projects(str(empty)) == []


def test_find_projects_nonexistent(tmp_path):
    assert find_projects(str(tmp_path / "nope")) == []


def test_resolve_package_path_empty(fake_aerostack):
    result = resolve_package_path("", str(fake_aerostack))
    assert result == str(fake_aerostack)


def test_resolve_package_path_projects(fake_aerostack):
    result = resolve_package_path("projects", str(fake_aerostack))
    assert result == str(fake_aerostack / "projects")


def test_resolve_package_path_project_name(fake_aerostack):
    result = resolve_package_path("gazebo", str(fake_aerostack))
    assert result == str(fake_aerostack / "projects" / "gazebo")


def test_resolve_package_path_exact(fake_aerostack):
    result = resolve_package_path("as2_core", str(fake_aerostack))
    assert result is not None
    assert result.endswith("as2_core")


def test_resolve_package_path_prefix_fallback(fake_aerostack):
    result = resolve_package_path("core", str(fake_aerostack))
    assert result is not None
    assert result.endswith("as2_core")


def test_resolve_package_path_not_found(fake_aerostack):
    result = resolve_package_path("nonexistent", str(fake_aerostack))
    assert result is None
