##############################################################################
# Copyright 2013 Laboratory for Advanced Computing at the University of Chicago

# 	      This file is part of udpipe by Joshua Miller

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#      http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions
# and limitations under the License.
##############################################################################

all: udpipe

udpipe:
	$(MAKE) -C udt/src/
	$(MAKE) -C src/

clean:
	$(MAKE) clean -C src/
	$(MAKE) clean -C udt/

install:
	$(MAKE) install -C udt/src/
	$(MAKE) install -C src/

.PHONY: install all udpipe