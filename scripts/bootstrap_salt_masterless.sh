#!/bin/bash

# Performs instructions at https://repo.saltstack.com/#ubuntu
# to set up SaltStack to be used in masterless mode. See
# https://docs.saltstack.com/en/latest/topics/tutorials/quickstart.html
# for more information.

# Import the SaltStack repository key.
wget -O - https://repo.saltstack.com/apt/ubuntu/16.04/amd64/latest/SALTSTACK-GPG-KEY.pub | apt-key add -

# Set up SaltStack apt repo.
echo "deb https://repo.saltstack.com/apt/ubuntu/14.04/amd64/latest trusty main" > /etc/apt/sources.list.d/saltstack.list

# Install salt-minion.
apt-get update
apt-get install salt-minion

# Write masterless configuration.
# Backup existing minion configuration.
rsync -a -v --ignore-existing /etc/salt/minion /etc/salt/minion.bak

perl -0777 -i -pe 's/#file_client: remote/file_client: local/igs' /etc/salt/minion
perl -0777 -i -pe 's/#file_roots:\n#  base:\n#    - \/srv\/salt/file_roots:\n  base:\n    - $ENV{HOME}\/Development\/deployment\/saltstack\/salt/igs' /etc/salt/minion
perl -0777 -i -pe 's/#pillar_roots:\n#  base:\n#    - \/srv\/pillar/pillar_roots:\n  base:\n    - $ENV{HOME}\/Development\/deployment\/saltstack\/pillar/igs' /etc/salt/minion

# Restart minion service.
service salt-minion restart
