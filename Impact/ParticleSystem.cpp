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


#include "stdafx.h"

namespace Impact {

#ifndef PARTICLES_WITH_SPRITES
  const float ParticleSystem::sHalfSize = 2.f;
#endif

  
  const float32 ParticleSystem::DefaultDensity = 1.f;
  const float32 ParticleSystem::DefaultFriction = 0.f;
  const float32 ParticleSystem::DefaultRestitution = 0.8f;

  const sf::Time ParticleSystem::sMaxAge = sf::milliseconds(1000);
  const sf::Color ParticleSystem::sColor = sf::Color::White;

  ParticleSystem::ParticleSystem(Game *game, const b2Vec2 &pos, bool ballCollisionEnabled, int count)
    : Body(Body::BodyType::Particle, game)
    , mParticles(count)
#ifndef PARTICLES_WITH_SPRITES
    , mVertices(sf::Quads, 4 * count)
#endif
  {
    setZIndex(Body::ZIndex::Foreground + 0);
    mName = std::string("ParticleSystem");
    setLifetime(sMaxAge);
    mTexture.loadFromFile("resources/images/particle.png");
    mShader.loadFromFile("resources/shaders/particlesystem.frag", sf::Shader::Fragment);
    mShader.setParameter("uTexture", sf::Shader::CurrentTexture);
    mShader.setParameter("uMaxAge", sMaxAge.asSeconds());

    boost::random::uniform_int_distribution<sf::Int32> randomLifetime(500, 1000);
    boost::random::uniform_real_distribution<float32> randomAngle(0.f, 2 * b2_pi);
    boost::random::uniform_real_distribution<float32> randomSpeed(2.f * Game::Scale, 5.f * Game::Scale);

    b2World *world = mGame->world();
    const int N = mParticles.size();
    for (int i = 0; i < N; ++i) {
      SimpleParticle &p = mParticles[i];
      p.dead = false;
      p.lifeTime = sf::milliseconds(randomLifetime(gRNG));
      p.sprite.setTexture(mTexture);
      mTexture.setRepeated(false);
      mTexture.setSmooth(false);
      p.sprite.setOrigin(.5f * mTexture.getSize().x, .5f * mTexture.getSize().y);

      b2BodyDef bd;
      bd.type = b2_dynamicBody;
      bd.position.Set(pos.x, pos.y);
      bd.fixedRotation = true;
      bd.bullet = false;
      bd.userData = this;
      bd.gravityScale = 5.f;
      bd.linearDamping = .2f;
      const b2Rot angle(randomAngle(gRNG));
      bd.linearVelocity = randomSpeed(gRNG) * b2Vec2(angle.c, angle.s);
      p.body = world->CreateBody(&bd);

      b2CircleShape circleShape;
      circleShape.m_radius = 1e-6f * Game::InvScale;

      b2FixtureDef fd;
      fd.density = DefaultDensity;
      fd.restitution = DefaultRestitution;
      fd.friction = DefaultFriction;
      fd.filter.categoryBits = Body::ParticleMask;
      fd.filter.maskBits = 0xffffU ^ Body::ParticleMask ^ Body::RacketMask;
      if (!ballCollisionEnabled)
        fd.filter.maskBits ^= Body::BallMask;
      fd.shape = &circleShape;
      p.body->CreateFixture(&fd);
    }
  }


  ParticleSystem::~ParticleSystem()
  {
    b2World *world = mGame->world();
    for (std::vector<SimpleParticle>::const_iterator p = mParticles.cbegin(); p != mParticles.cend(); ++p) {
      if (!p->dead)
        world->DestroyBody(p->body);
    }
  }


  void ParticleSystem::setBallCollisionEnabled(bool ballCollisionEnabled)
  {
    for (std::vector<SimpleParticle>::iterator p = mParticles.begin(); p != mParticles.end(); ++p) {
      b2Fixture *fixture = p->body->GetFixtureList();
      b2Filter filter = fixture->GetFilterData();
      if (ballCollisionEnabled)
        filter.maskBits |= Body::BallMask;
      else
        filter.maskBits &= ~Body::BallMask;
      fixture->SetFilterData(filter);
    }
  }


  void ParticleSystem::onUpdate(float)
  {
    bool allDead = true;
    const int N = mParticles.size();
#pragma omp parallel for reduction(&:allDead)
    for (int i = 0; i < N; ++i) {
      SimpleParticle &p = mParticles[i];
      if (age() > p.lifeTime && !p.dead) {
        p.dead = true;
        mGame->world()->DestroyBody(p.body);
      }
      else {
        const b2Transform &tx = p.body->GetTransform();
        p.sprite.setPosition(float(Game::Scale) * sf::Vector2f(tx.p.x, tx.p.y));
        p.sprite.setRotation(rad2deg(tx.q.GetAngle()));
      }
      allDead &= p.dead;
    }
    mShader.setParameter("uAge", age().asSeconds());
    if (allDead || overAge())
      this->kill();
  }


  void ParticleSystem::onDraw(sf::RenderTarget &target, sf::RenderStates states) const
  {
    states.shader = &mShader;
    for (std::vector<SimpleParticle>::const_iterator p = mParticles.cbegin(); p != mParticles.cend(); ++p)
      if (!p->dead)
        target.draw(p->sprite, states);
  }

}
