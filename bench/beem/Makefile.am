## Copyright (C) 2011 Laboratoire de Recherche et
## Développement de l'Epita (LRDE).
##
## This file is part of Spot, a model checking library.
##
## Spot is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## Spot is distributed in the hope that it will be useful, but WITHOUT
## ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
## or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
## License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Spot; see the file COPYING.  If not, write to the Free
## Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
## 02111-1307, USA.


models_file = models_data.tar.gz
models_url = http://anna.fi.muni.cz/models/$(models_file)
# Download the model using either wget or curl.
$(models_file):
	wget $(models_url) -O $@ || curl $(models_url) -o $@

data/stamp: $(models_file)
	GZIP=$(GZIP_ENV) gzip -dc $(models_file) | $(am__untar)
	touch data/stamp
