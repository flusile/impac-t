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

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>

#include <zlib.h>

#if defined(WIN32)
#include "../zip-utils/unzip.h"
#endif
#if defined(LINUX_AMD64)
#include <libgen.h>
extern "C" {
#include "../minizip/miniunz.h"
}
#endif

#include "sha1.h"

#if defined(WIN32)
#include <Shlwapi.h>
#endif


// #define NDEBUG 1

namespace Impact {

  const float32 Level::DefaultGravity = 9.81f;
  const float32 Level::DefaultWallRestitution = 1.f;

  Level::Level(void)
    : mBackgroundColor(sf::Color::Black)
    , mBackgroundVisible(true)
    , mFirstGID(0)
    , mNumTilesX(40)
    , mNumTilesY(25)
    , mTileWidth(16)
    , mTileHeight(16)
    , mLevelNum(0)
    , mGravity(DefaultGravity)
    , mWallRestitution(DefaultWallRestitution)
    , mExplosionParticlesCollideWithBall(false)
    , mKillingsPerKillingSpree(Game::DefaultKillingsPerKillingSpree)
    , mKillingSpreeBonus(Game::DefaultKillingSpreeBonus)
    , mKillingSpreeInterval(Game::DefaultKillingSpreeInterval)
    , mSuccessfullyLoaded(false)
    , mMusic(nullptr)
  {
    // ...
  }


  Level::Level(int num)
    : Level()
  {
    set(num, true);
  }


  Level::Level(const Level &other)
    : mBackgroundColor(other.mBackgroundColor)
    , mFirstGID(other.mFirstGID)
    , mMapData(other.mMapData)
    , mNumTilesX(other.mNumTilesX)
    , mNumTilesY(other.mNumTilesY)
    , mTileWidth(other.mTileWidth)
    , mTileHeight(other.mTileHeight)
    , mLevelNum(other.mLevelNum)
    , mGravity(other.mGravity)
    , mWallRestitution(other.mWallRestitution)
    , mExplosionParticlesCollideWithBall(other.mExplosionParticlesCollideWithBall)
    , mKillingsPerKillingSpree(other.mKillingsPerKillingSpree)
    , mKillingSpreeBonus(other.mKillingSpreeBonus)
    , mKillingSpreeInterval(other.mKillingSpreeInterval)
    , mSuccessfullyLoaded(other.mSuccessfullyLoaded)
    , mName(other.mName)
    , mCredits(other.mCredits)
    , mAuthor(other.mAuthor)
    , mCopyright(other.mCopyright)
    , mMusic(other.mMusic)
  {
    // ...
  }


  Level::~Level()
  {
    clear();
  }


  bool Level::set(int level, bool doLoad)
  {
    mSuccessfullyLoaded = false;
    mLevelNum = level;
    if (mLevelNum > 0 && doLoad)
      load();
    return mSuccessfullyLoaded;
  }


  bool Level::gotoNext(void)
  {
    return set(mLevelNum + 1, true);
  }


  bool Level::calcSHA1(const std::string &filename)
  {
    std::ifstream is;
    is.open(filename, std::ios::binary);
    if (is.is_open()) {
      is.seekg(0, std::ios::end);
      int nBytes = int(is.tellg());
      is.seekg(0, std::ios::beg);
      char *buf = new char[nBytes];
      is.read(buf, nBytes);
      is.close();
      unsigned char hash[20];
      sha1::calc(buf, nBytes, hash);
      std::stringstream strBuf;
      for (int i = 0; i < 20; ++i)
        strBuf << std::hex << std::setw(2) << std::setfill('0') << short(hash[i]);
      mSHA1 = strBuf.str();
      delete[] buf;
      mBase62Name = base62_encode<boost::multiprecision::uint256_t>(reinterpret_cast<uint8_t*>(hash), sizeof(hash));
      return true;
    }
    return false;
  }


  void Level::load(void)
  {
    std::string levelFilename;
    std::ostringstream levelStrBuf;
    levelStrBuf << std::setw(4) << std::setfill('0') << mLevelNum;
    const std::string levelStr = levelStrBuf.str();
    levelFilename = gLocalSettings().levelsDir() + "/" + levelStr + ".zip";
    loadZip(levelFilename);
  }


#pragma warning(disable : 4503)
  void Level::loadZip(const std::string &zipFilename)
  {
    mSuccessfullyLoaded = false;
    bool ok = true;

    std::string levelPath;
    std::string levelFilename;

    safeDelete(mMusic);

#if defined(WIN32)
    char szPath[MAX_PATH];
    strcpy_s(szPath, MAX_PATH, zipFilename.c_str());
    PathStripPath(szPath);
    PathRemoveExtension(szPath);
    mName = szPath;
#elif defined(LINUX_AMD64)
    char szPath[MAX_PATH];
    strncpy(szPath, zipFilename.c_str(), MAX_PATH);
    char* fName = basename(szPath);
    char* dot = index(fName, '.');
    if (dot) {
       *dot = 0;
    }
    mName = basename(fName);
#endif

#ifndef NDEBUG
    std::cout << "LEVEL NAME: " << mName << std::endl;
#endif

#if defined(WIN32)
    HZIP hz = OpenZip(zipFilename.c_str(), nullptr);
    if (hz) {
      levelPath = gLocalSettings().levelsDir() + "/" + mName;
      SetUnzipBaseDir(hz, levelPath.c_str());
      ZIPENTRY ze;
      GetZipItem(hz, -1, &ze);
      int nItems = ze.index;
      for (int i = 0; i < nItems; ++i) {
        GetZipItem(hz, i, &ze);
        UnzipItem(hz, i, ze.name);
        std::string currentItemName = ze.name;
        if (boost::algorithm::ends_with(currentItemName, ".tmx")) {
          levelFilename = levelPath + "/" + currentItemName;
        }
        else if (boost::algorithm::ends_with(currentItemName, ".ogg")) {
          mMusic = new sf::Music;
          if (mMusic != nullptr) {
            bool musicLoaded = mMusic->openFromFile(levelPath + "/" + currentItemName);
            if (musicLoaded) {
              mMusic->setLoop(true);
              mMusic->setVolume(gLocalSettings().musicVolume());
            }
          }
        }
      }
      CloseZip(hz);
      calcSHA1(zipFilename);
    }
#elif defined(LINUX_AMD64)
    unzFile hz = unzOpen(zipFilename.c_str());
    if (hz) {
      int rc;
      levelPath = gLocalSettings().levelsDir() + "/" + mName;
      char curwd[MAX_PATH];
      const char *path = getcwd(curwd, MAX_PATH);
      mkdir(levelPath.c_str(), 0775);
      rc = chdir(levelPath.c_str());
      if (rc != 0)
        return;
      unz_global_info gInfo;
      unzGetGlobalInfo(hz, &gInfo);
      int nItems = gInfo.number_entry;
      int extractWithoutPath = 0;
      int extractOverwrite = 1;
      for (int i = 0; i < nItems; ++i) {
        char zeName[MAX_PATH];
        unz_file_info fi;
        unzGetCurrentFileInfo(hz, &fi, zeName, MAX_PATH, NULL, 0, NULL, 0);
        do_extract_currentfile(hz, &extractWithoutPath, &extractOverwrite, NULL);
        std::string currentItemName = zeName;
        if (boost::algorithm::ends_with(currentItemName, ".tmx")) {
          levelFilename = levelPath + "/" + currentItemName;
        }
        else if (boost::algorithm::ends_with(currentItemName, ".ogg")) {
          mMusic = new sf::Music;
          if (mMusic != nullptr) {
            bool musicLoaded = mMusic->openFromFile(levelPath + "/" + currentItemName);
            if (musicLoaded) {
              mMusic->setLoop(true);
              mMusic->setVolume(gLocalSettings().musicVolume());
            }
          }
        }
        if ((i+1)<nItems) {
          unzGoToNextFile(hz);
        }
      }
      rc = chdir(curwd);
      unzClose(hz);
  }
#endif

    ok = fileExists(levelFilename);
    if (!ok)
      return;

    mBackgroundImageOpacity = 1.f;
    boost::property_tree::ptree pt;
    try {
      boost::property_tree::xml_parser::read_xml(levelFilename, pt);
    }
    catch (const boost::property_tree::xml_parser::xml_parser_error &ex) {
      std::cerr << "XML parser error: " << ex.what() << " (line " << ex.line() << ")" << std::endl;
      ok = false;
    }

    if (!ok)
      return;

    try { // evaluate level properties
      mMapData.clear();
      mGravity = DefaultGravity;
      mWallRestitution = DefaultWallRestitution;
      mCredits = std::string();
      mAuthor = std::string();
      mCopyright = std::string();
      mInfo = std::string();
      mBackgroundColor = sf::Color::Black;
      mKillingsPerKillingSpree = Game::DefaultKillingsPerKillingSpree;
      mKillingSpreeBonus = Game::DefaultKillingSpreeBonus;
      mKillingSpreeInterval = Game::DefaultKillingSpreeInterval;
      mExplosionParticlesCollideWithBall = false;
      const boost::property_tree::ptree &layerProperties = pt.get_child("map.properties");
      boost::property_tree::ptree::const_iterator pi;
      for (pi = layerProperties.begin(); pi != layerProperties.end(); ++pi) {
        boost::property_tree::ptree property = pi->second;
        if (pi->first == "property") {
          std::string propName = property.get<std::string>("<xmlattr>.name");
          boost::algorithm::to_lower(propName);
          if (propName == "credits") {
            mCredits = property.get<std::string>("<xmlattr>.value", std::string());
          }
          else if (propName == "author") {
            mAuthor = property.get<std::string>("<xmlattr>.value", std::string());
          }
          else if (propName == "copyright") {
            mCopyright = property.get<std::string>("<xmlattr>.value", std::string());
          }
          else if (propName == "name") {
            mName = property.get<std::string>("<xmlattr>.value", std::string());
          }
          else if (propName == "gravity") {
            mGravity = property.get<float32>("<xmlattr>.value", 9.81f);
          }
          else if (propName == "wallrestitution") {
            mWallRestitution = property.get<float32>("<xmlattr>.value", 1.f);
          } 
          else if (propName == "explosionparticlescollidewithball") {
            mExplosionParticlesCollideWithBall = property.get<bool>("<xmlattr>.value", false);
          }
          else if (propName == "killingspreebonus") {
            mKillingSpreeBonus = property.get<int>("<xmlattr>.value", Game::DefaultKillingSpreeBonus);
          }
          else if (propName == "killingspreeinterval") {
            mKillingSpreeInterval = sf::milliseconds(property.get<int>("<xmlattr>.value", Game::DefaultKillingSpreeInterval.asMilliseconds()));
          }
          else if (propName == "killingsperkillingspree") {
            mKillingsPerKillingSpree = property.get<int>("<xmlattr>.value", Game::DefaultKillingsPerKillingSpree);
          }
        }
      }
    } catch (boost::property_tree::ptree_error &e) { UNUSED(e); }

    try {
      const std::string &mapDataB64 = pt.get<std::string>("map.layer.data");
      mTileWidth = pt.get<int>("map.<xmlattr>.tilewidth");
      mTileHeight = pt.get<int>("map.<xmlattr>.tileheight");
      mNumTilesX = pt.get<int>("map.<xmlattr>.width");
      mNumTilesY = pt.get<int>("map.<xmlattr>.height");
      try {
        std::string bgColor = pt.get<std::string>("map.<xmlattr>.backgroundcolor");
        int r = 0, g = 0, b = 0;
        if (bgColor.size() == 7 && bgColor[0] == '#') {
          bgColor.erase(0, 1);
          const uint32_t rgb = std::stoul(bgColor, 0, 16);
          r = (rgb >> 16) & 0xff;
          g = (rgb >> 8) & 0xff;
          b = rgb & 0xff;
          mBackgroundColor = sf::Color(r, g, b, 255);
        }
      } catch (boost::property_tree::ptree_error &e) { UNUSED(e); }

      uint8_t *compressed = nullptr;
      uLong compressedSize = 0UL;
      base64_decode(mapDataB64, compressed, compressedSize);
      if (compressed != nullptr && compressedSize > 0) {
        static const size_t CHUNKSIZE = 128 * 1024; // sizeof(uint32_t) * Game::DefaultPlaygroundWidth * Game::DefaultPlaygroundHeight;
        uint32_t *mapData = new uint32_t[CHUNKSIZE / sizeof(uint32_t)];
        if (mapData != nullptr) {
          uLongf mapDataSize = CHUNKSIZE;
          int rc = uncompress(reinterpret_cast<Bytef*>(mapData), &mapDataSize, reinterpret_cast<Bytef*>(compressed), compressedSize);
          if (rc == Z_OK) {
            for (uLongf i = 0; i < mapDataSize; ++i) {
              mMapData.push_back(*(mapData + i));
            }
            delete [] mapData;
          }
          else {
            ok = false;
            if (rc == Z_DATA_ERROR)
              std::cerr << "Inflating map data failed: Z_DATA_ERROR" << std::endl;
            else if (rc == Z_MEM_ERROR)
              std::cerr << "Inflating map data failed: Z_MEM_ERROR" << std::endl;
            else if (rc == Z_BUF_ERROR)
              std::cerr << "Inflating map data failed: Z_BUF_ERROR " << std::endl;
            else
              std::cerr << "Inflating map data failed with error code " << rc << std::endl;
          }
        }
        delete[] compressed;
      }

      if (!ok)
        return;

      try {
        mBackgroundVisible = pt.get<bool>("map.layer.imagelayer.<xmlattr>.visible", true);
        if (mBackgroundVisible) {
          const std::string &backgroundTextureFilename = levelPath + "/" + pt.get<std::string>("map.imagelayer.image.<xmlattr>.source");
          mBackgroundTexture.loadFromFile(backgroundTextureFilename);
          mBackgroundSprite.setTexture(mBackgroundTexture);
          mBackgroundImageOpacity = pt.get<float>("map.imagelayer.<xmlattr>.opacity", 1.f);
          mBackgroundSprite.setColor(sf::Color(255, 255, 255, sf::Uint8(mBackgroundImageOpacity * 0xff)));
        }
      }
      catch (boost::property_tree::ptree_error &e) { UNUSED(e); }

      mBoundary = Boundary();
      try {
        const boost::property_tree::ptree &object = pt.get_child("map.objectgroup.object");
        mBoundary.left = object.get<int>("<xmlattr>.x");
        mBoundary.top = object.get<int>("<xmlattr>.y");
        mBoundary.right = mBoundary.left + object.get<int>("<xmlattr>.width");
        mBoundary.bottom = mBoundary.top + object.get<int>("<xmlattr>.height");
        mBoundary.valid = true;
      } catch (boost::property_tree::ptree_error &e) { UNUSED(e); }

      const boost::property_tree::ptree &tileset = pt.get_child("map.tileset");
      mFirstGID = tileset.get<uint32_t>("<xmlattr>.firstgid");
      mTiles.resize(tileset.count("tile") + mFirstGID);
      boost::property_tree::ptree::const_iterator ti;
      for (ti = tileset.begin(); ti != tileset.end(); ++ti) {
        boost::property_tree::ptree tile = ti->second;
        if (ti->first == "tile") {
          const int id = mFirstGID + tile.get<int>("<xmlattr>.id");
          mTiles.resize(id + 1);
          TileParam tileParam;
          const std::string &filename = levelPath + "/" + tile.get<std::string>("image.<xmlattr>.source");
          ok = tileParam.texture.loadFromFile(filename);
          if (!ok)
            return;
          const boost::property_tree::ptree &tileProperties = tile.get_child("properties");
          boost::property_tree::ptree::const_iterator pi;
          for (pi = tileProperties.begin(); pi != tileProperties.end(); ++pi) {
            boost::property_tree::ptree property = pi->second;
            if (pi->first == "property") {
              try {
                std::string propName = property.get<std::string>("<xmlattr>.name");
                boost::algorithm::to_lower(propName);
                if (propName == "name") {
                  tileParam.textureName = property.get<std::string>("<xmlattr>.value");
                }
                //MOD Property1
                else if (propName == "points") {
                  tileParam.score = property.get<int64_t>("<xmlattr>.value", 0);
                }
                else if (propName == "fixed") {
                  tileParam.fixed = property.get<bool>("<xmlattr>.value", false);
                }
                //MOD Property2
                else if (propName == "friction") {
                  tileParam.friction = property.get<float32>("<xmlattr>.value", .5f);
                }
                else if (propName == "lineardamping") {
                  tileParam.linearDamping = property.get<float32>("<xmlattr>.value", 5.f);
                }
                else if (propName == "angulardamping") {
                  tileParam.angularDamping = property.get<float32>("<xmlattr>.value", .4f);
                }
                else if (propName == "restitution") {
                  tileParam.restitution = property.get<float32>("<xmlattr>.value", 1.f);
                }
                else if (propName == "density") {
                  tileParam.density = property.get<float32>("<xmlattr>.value", 20.f);
                }
                else if (propName == "gravityscale") {
                  tileParam.gravityScale = property.get<float32>("<xmlattr>.value", 1.f);
                }
                else if (propName == "scalegravityby") {
                  tileParam.scaleGravityBy = property.get<float32>("<xmlattr>.value", 0.f);
                }
                else if (propName == "scalegravityseconds") {
                  tileParam.scaleGravityDuration = sf::seconds(property.get<float32>("<xmlattr>.value", 0.f));
                }
                else if (propName == "scaleballdensityby") {
                  tileParam.scaleBallDensityBy = property.get<float32>("<xmlattr>.value", 0.f);
                }
                else if (propName == "scaleballdensityseconds") {
                  tileParam.scaleBallDensityDuration = sf::seconds(property.get<float32>("<xmlattr>.value", 0.f));
                }
                else if (propName == "minimumhitimpulse") {
                  tileParam.minimumHitImpulse = property.get<int>("<xmlattr>.value", 5);
                }
                else if (propName == "minimumkillimpulse") {
                  tileParam.minimumKillImpulse = property.get<int>("<xmlattr>.value", 50);
                }
                else if (propName == "smooth") {
                  tileParam.smooth = property.get<bool>("<xmlattr>.value", true);
                }
                else if (propName == "earthquakeseconds") {
                  tileParam.earthquakeDuration = sf::seconds(property.get<float32>("<xmlattr>.value", 0));
                }
                else if (propName == "earthquakeintensity") {
                  tileParam.earthquakeIntensity = .05f * property.get<float32>("<xmlattr>.value", 0.f) ;
                }
                else if (propName == "impulse") {
                  tileParam.bumperImpulse = property.get<float32>("<xmlattr>.value", 20.f);
                }
                else if (propName == "multiball") {
                  tileParam.multiball = property.get<bool>("<xmlattr>.value", false);
                }
                else if (propName == "shape") {
                  tileParam.shapeType = property.get<BodyShapeType>("<xmlattr>.value", BodyShapeType::CircleShape);
                }
              }
              catch (boost::property_tree::ptree_error &e) { UNUSED(e); }
            }
          }
          if (!tileParam.fixed.isValid())
            tileParam.fixed = (tileParam.textureName == Wall::Name) || (tileParam.textureName == Bumper::Name);
          mTiles[id] = tileParam;
        }
      }
    }
    catch (boost::property_tree::ptree_error &e) {
      std::cerr << "Error parsing TMX file: " << e.what() << std::endl;
      ok = false;
    }

    mSuccessfullyLoaded = ok;
#ifndef NDEBUG
    std::cout << "Level " << (mSuccessfullyLoaded ? "loaded." : "NOT loaded.") << std::endl;
    if (mSuccessfullyLoaded)
      std::cout <<
      "            _\n"
      "           /(|\n"
      "          (  :\n"
      "         __\\  \\  _____\n"
      "       (____)  `|\n"
      "      (____)|   |\n"
      "       (____).__|\n"
      "        (___)__.|_____\n"
      << std::endl;
#endif
  }


  void Level::clear(void)
  {
    mTiles.clear();
  }


  int Level::bodyIndexByTextureName(const std::string &name) const
  {
    const int N = mTiles.size();
    for (int i = 0; i < N; ++i)
      if (name == mTiles.at(i).textureName)
        return i;
    return -1;
  }


  const sf::Texture &Level::texture(const std::string &name) const
  {
    const int index = bodyIndexByTextureName(name);
    if (index < 0)
      throw "Bad texture name: '" + name + "'";
    return mTiles.at(index).texture;
  }


  uint32_t *const Level::mapDataScanLine(int y)
  {
    return mMapData.data() + y * mNumTilesX;
  }


  const TileParam &Level::tileParam(int index) const
  {
    return mTiles.at(index);
  }

}
