import os
import xml.etree.ElementTree as ET


def find_packages(path):
    results = []
    for root, _dirs, files in os.walk(path):
        if "package.xml" in files:
            xml_path = os.path.join(root, "package.xml")
            try:
                tree = ET.parse(xml_path)
                xml_root = tree.getroot()
                export = xml_root.find("export")
                if export is not None:
                    build_type = export.find("build_type")
                    if build_type is not None and "ament" in build_type.text:
                        name_el = xml_root.find("name")
                        if name_el is not None:
                            results.append((name_el.text.strip(), root))
            except ET.ParseError:
                continue
    results.sort(key=lambda x: x[0])
    return results


def find_projects(path):
    if not os.path.isdir(path):
        return []
    entries = sorted(
        e for e in os.listdir(path)
        if os.path.isdir(os.path.join(path, e))
    )
    return [(e, os.path.join(path, e)) for e in entries]


def resolve_package_path(name, aerostack2_path):
    projects_path = os.path.join(aerostack2_path, "projects")

    if not name:
        return aerostack2_path

    if name == "projects":
        return projects_path

    project_dir = os.path.join(projects_path, name)
    if os.path.isdir(project_dir):
        return project_dir

    packages = find_packages(aerostack2_path)
    pkg_map = {n: p for n, p in packages}

    if name in pkg_map:
        return pkg_map[name]

    prefixed = f"as2_{name}"
    if prefixed in pkg_map:
        return pkg_map[prefixed]

    return None
