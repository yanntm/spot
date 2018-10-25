// -*- coding: utf-8 -*-
// Copyright (C) 2018 Laboratoire de Recherche et Developpement de l'Epita
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <atomic>
#include <mutex>
#include <queue>

namespace spot
{
	template<typename E>
	class s_queue
	{
		public:
		s_queue():
			q_(), l_()
		{ }

		void push(E e)
		{
			//std::cout << "push() (queue.size = " << q_.size() << ")" << std::endl;
			std::lock_guard<std::mutex> lock(l_);
			q_.push(e);
		}

		E pop()
		{
			//std::cout << "pop() (queue.size = " << q_.size() << ")" << std::endl;
			std::lock_guard<std::mutex> lock(l_);
			if (q_.size() == 0)
				return nullptr;

			auto e = q_.front();
			q_.pop();
			return e;
		}

		private:
			std::queue<E> q_;
			std::mutex l_;
	};
}
