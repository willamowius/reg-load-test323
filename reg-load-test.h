/*
 * reg-load-test.h
 *
 * Registration load tester
 *
 * Copyright (c) 2026 Jan Willamowius <jan@willamowius.de>
 *
 */


#include <ptlib.h>
#include <h323.h>
#include <list>

using namespace std;

#define MAJOR_VERSION 0
#define MINOR_VERSION 1
#define BUILD_TYPE    BetaCode
#define BUILD_NUMBER 0

///////////////////////////////////////////////////////////////////////////////

    class LoadTestEndpoint : public H323EndPoint
{
public:
    LoadTestEndpoint();
};

///////////////////////////////////////////////////////////////////////////////
    
class LoadTestProcess : public PProcess
{
public:
    LoadTestProcess();
    void Main();
    static void Shutdown();

protected:
    static list<LoadTestEndpoint *> * m_endpoints;
};
