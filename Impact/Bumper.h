/*

Copyright (c) 2015 Oliver Lau <ola@ct.de>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/


#ifndef __BODYBUMPER_H_
#define __BODYBUMPER_H_

#include "Body.h"
#include "Impact.h"

namespace Impact {

  class Bumper : public Body
  {
  public:
    Bumper(int index, Game *game, const TileParam &tileParam);

    // Body implementation
    virtual void onUpdate(float elapsedSeconds);
    virtual void onDraw(sf::RenderTarget &target, sf::RenderStates states) const;
    virtual BodyType type(void) const { return Body::BodyType::Bumper; }

    static const std::string Name;

    virtual void setPosition(float32 x, float32 y);
    virtual void setPosition(const b2Vec2 &pos);

    void activate(void);

  private:
    sf::Clock mActivationTimer;
    bool mActivated;

  };

}

#endif // __BODYBUMPER_H_

