#ifndef _LINKED_OBJECTS_H_
#define _LINKED_OBJECTS_H_

#include <stdio.h>
#include <stdlib.h>

#include <Polygon/polygon.hxx>

using std::string;

class Windsock
{
public:
    Windsock(char* def);

    double lat;
    double lon;
    int lit;

    SGGeod GetLoc()
    {
        return SGGeod::fromDeg(lon, lat);
    }

    bool IsLit()
    {
        return (lit == 1) ? true : false;
    }
};

typedef std::vector <Windsock *> WindsockList;


class Beacon
{
public:
    Beacon(char* def);

    double lat;
    double lon;
    int code;

    SGGeod GetLoc()
    {
        return SGGeod::fromDeg(lon, lat);
    }

    int GetCode()
    {
        return code;
    }
};

typedef std::vector <Beacon *> BeaconList;

class Sign
{
public:
    Sign(char* def);

    double lat;
    double lon;
    double heading;
    int    reserved;
    int    size;
    string sgn_def;

    SGGeod GetLoc()
    {
        return SGGeod::fromDeg(lon, lat);
    }

    double GetHeading()
    {
        return heading;
    }

    string GetDefinition()
    {
        return sgn_def;
    }

    int GetSize()
    {
        return size;
    }
};

typedef std::vector <Sign *> SignList;

#endif
