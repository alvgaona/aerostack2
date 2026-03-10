from setuptools import setup, find_packages

package_name = 'as2_cli'

setup(
    name=package_name,
    version='1.1.3',
    packages=find_packages(exclude=[
        'test', 'build', 'install', 'log', 'resource',
        'build.*', 'install.*', 'log.*', 'resource.*',
    ]),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools', 'click'],
    zip_safe=True,
    maintainer='CVAR-UPM',
    maintainer_email='cvar.upm3@gmail.com',
    description='AS2 CLI Package',
    license='BSD-3-Clause',
    entry_points={
        'console_scripts': [
            'as2 = as2_cli.cli:main',
        ],
    },
)
