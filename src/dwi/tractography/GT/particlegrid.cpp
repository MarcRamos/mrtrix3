/*
 * Copyright (c) 2008-2016 the MRtrix3 contributors
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/
 * 
 * MRtrix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * For more details, see www.mrtrix.org
 * 
 */

#include "dwi/tractography/GT/particlegrid.h"

namespace MR {
  namespace DWI {
    namespace Tractography {
      namespace GT {
        
        
        void ParticleGrid::add(const Point_t &pos, const Point_t &dir)
        {
          Particle* p = pool.create(pos, dir);
          unsigned int gidx = pos2idx(pos);
          grid[gidx].push_back(p);
          std::lock_guard<std::mutex> lock (mutex);
          list.push_back(p);
        }
        
        void ParticleGrid::shift(Particle *p, const Point_t& pos, const Point_t& dir)
        {
          unsigned int gidx0 = pos2idx(p->getPosition());
          unsigned int gidx1 = pos2idx(pos);
          for (ParticleVectorType::iterator it = grid[gidx0].begin(); it != grid[gidx0].end(); ++it) {
            if (*it == p) {
              grid[gidx0].erase(it);
              break;
            }
          }
          p->setPosition(pos);
          p->setDirection(dir);
          grid[gidx1].push_back(p);
        }
        
        void ParticleGrid::remove(const size_t idx)
        {
          std::lock_guard<std::mutex> lock (mutex);
          Particle* p = list[idx];
          unsigned int gidx0 = pos2idx(p->getPosition());
          for (ParticleVectorType::iterator it = grid[gidx0].begin(); it != grid[gidx0].end(); ++it) {
            if (*it == p) {
              grid[gidx0].erase(it);
              break;
            }
          }
          list[idx] = list.back();    // FIXME Not thread safe if last element is in use by another _delete_ proposal!  
          list.pop_back();            // May corrupt datastructure, but program won't crash. Ignore for now.
          pool.destroy(p);
        }
        
        void ParticleGrid::clear()
        {
          grid.clear();
          std::lock_guard<std::mutex> lock (mutex);
          for (ParticleVectorType::iterator it = list.begin(); it != list.end(); ++it)
            pool.destroy(*it);
          list.clear();
        }
        
        const ParticleGrid::ParticleVectorType* ParticleGrid::at(const ssize_t x, const ssize_t y, const ssize_t z) const
        {
          if ((x < 0) || (size_t(x) >= dims[0]) || (y < 0) || (size_t(y) >= dims[1]) || (z < 0) || (size_t(z) >= dims[2]))  // out of bounds
            return nullptr;
          return &grid[xyz2idx(x, y, z)];
        }
        
        Particle* ParticleGrid::getRandom(size_t& idx)
        {
          if (list.empty())
            return nullptr;
          std::uniform_int_distribution<size_t> dist(0, list.size()-1);
          idx = dist(rng);
          return list[idx];
        }
        
        void ParticleGrid::exportTracks(Tractography::Writer<float> &writer)
        {
          std::lock_guard<std::mutex> lock (mutex);
          // Initialise
          Particle* par;
          Particle* nextpar;
          int alpha = 0;
          std::vector<Point_t> track;
          // Loop through all unvisited particles
          for (ParticleVectorType::iterator it = list.begin(); it != list.end(); ++it) 
          {
            par = (*it);
            if (!par->isVisited())
            {
              par->setVisited(true);
              // forward
              track.push_back(par->getPosition());
              alpha = +1;
              while ((alpha == +1) ? par->hasSuccessor() : par->hasPredecessor())
              {
                nextpar = (alpha == +1) ? par->getSuccessor() : par->getPredecessor();
                alpha = (nextpar->getPredecessor() == par) ? +1 : -1;
                track.push_back(nextpar->getPosition());
                nextpar->setVisited(true);
                par = nextpar;
              }
              track.push_back(par->getEndPoint(alpha));
              // backward
              par = (*it);
              std::reverse(track.begin(), track.end());
              alpha = -1;
              while ((alpha == +1) ? par->hasSuccessor() : par->hasPredecessor())
              {
                nextpar = (alpha == +1) ? par->getSuccessor() : par->getPredecessor();
                alpha = (nextpar->getPredecessor() == par) ? +1 : -1;
                track.push_back(nextpar->getPosition());
                nextpar->setVisited(true);
                par = nextpar;
              }
              track.push_back(par->getEndPoint(alpha));
              if (track.size() > 1)
                writer(track);
              track.clear();
            }
          }
          // Free all particle locks
          for (ParticleVectorType::iterator it = list.begin(); it != list.end(); ++it)
            (*it)->setVisited(false);
        }
        

      }
    }
  }
}
