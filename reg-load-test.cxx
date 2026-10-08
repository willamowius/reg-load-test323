/*
 * reg-load-test.cxx
 *
 * Registration load tester
 *
 * Copyright (c) 2026 Jan Willamowius <jan@willamowius.de>
 *
 */

#include "reg-load-test.h"
#include <signal.h>
#include <ptclib/random.h>

PCREATE_PROCESS(LoadTestProcess);

const unsigned REGISTRATION_RETRIES = 3;           // retries after a failed registration
const unsigned REGISTRATION_RETRY_DELAY = 3000;    // wait between retries in ms

void UnixShutdownHandler(int sig)
{
	LoadTestProcess::Shutdown();
	exit(1);
}

///////////////////////////////////////////////////////////////////////////////

bool SplitAddress(const PString & addr, PString & host, WORD & port)
{
    if (addr.IsEmpty())
        return false;

    PINDEX lastChar = addr.GetLength() - 1;
    PINDEX colon = addr.FindLast(':');

    // No port separator -> hostname / IPv4
    if (colon == P_MAX_INDEX) {
        host = addr(0, lastChar);
        return true;
    }

    // Bracketed IPv6
    if (addr[0] == '[') {
        PINDEX close = addr.Find(']');
        if (close == P_MAX_INDEX || close == 1)
            return false;

        if (close < lastChar) {
            if (addr[close + 1] != ':' || colon == lastChar)
                return false;

            unsigned p = addr.Mid(colon + 1, lastChar).AsUnsigned();
            if (p > 65535)
                return false;

            port = (WORD)p;
        }

        host = addr.Mid(1, close - 1);
        return true;
    }

    // Single ':' => IPv4/domain with port
    if (addr.Find(':') == colon) {
        if (colon == lastChar)
            return false;

        unsigned p = addr.Mid(colon + 1, lastChar).AsUnsigned();
        if (p > 65535)
            return false;

        port = (WORD)p;
        host = addr.Left(colon);
        return true;
    }

    // Raw IPv6
    host = addr(0, lastChar);
    return true;
}

list<LoadTestEndpoint *> * LoadTestProcess::m_endpoints = NULL;

LoadTestProcess::LoadTestProcess()
  : PProcess("Registration Load Tester", "LoadTestProcess", MAJOR_VERSION, MINOR_VERSION, BUILD_TYPE, BUILD_NUMBER)
{
}

void LoadTestProcess::Main()
{
    PArgList & args = GetArguments();
    args.Parse(
             "b-baseport:"  // base port for H.323 listener, never used
             "c-count:"     // number of endpoints to create
             "d-delay:"     // delay between registrations in ms
             "g-gatekeeper:"
             "i-interface:"
             "o-output:"
             "p-password:"
             "s-servername:"
             "t-trace."
             "u-usernameprefix:"
#ifdef H323_H46018
             "-h46018enable."
#endif
             , FALSE);

    PTrace::Initialise(args.GetOptionCount('t'),
                     args.HasOption('o') ? (const char *)args.GetOptionString('o') : NULL,
		             PTrace::DateAndTime | PTrace::TraceLevel | PTrace::FileAndLine);

    signal(SIGTERM, UnixShutdownHandler);
    signal(SIGINT, UnixShutdownHandler);
    signal(SIGQUIT, UnixShutdownHandler);

    unsigned num_enpoints = args.HasOption('c') ? args.GetOptionString('c').AsUnsigned() : 1;
    if (num_enpoints < 1 || num_enpoints > 1000) {
        cout << "Number of endpoints must be between 1 and 1000" << endl;
        return;
    }
    PString namePrefix = args.HasOption('u') ? args.GetOptionString('u') : "ep";
    PString serverName = args.HasOption('s') ? args.GetOptionString('s') : ""; // default is a 20 char random string
    if (serverName.IsEmpty()) {
        for (unsigned i = 0; i < 20; ++i) {
            serverName += (char)('a' + PRandom::Number(25));
        }
    }

    unsigned delayBetweenRegistrations = args.HasOption('d') ? args.GetOptionString('d').AsUnsigned() : 100; // default 100 ms

    PIPSocket::Address interfaceAddress(INADDR_ANY);
    WORD listenPort = args.HasOption('b') ? args.GetOptionString('b').AsUnsigned() : PRandom::Number(10, 50) * 1000; // base TCP port for H.323 listeners, never used
    if (args.HasOption('i')) {
        PString interface = args.GetOptionString('i');
        if (!SplitAddress(interface, interface, listenPort)) {
            cout << "Could not parse param of -i \"" << interface << "\" to ip address and port." << endl;
            return;
        }
        interfaceAddress = interface;
    }

    PString gkAddr = args.GetOptionString('g');
    if (gkAddr.IsEmpty()) {
        cout << "No gatekeeper address specified, terminating." << endl;
        return;
    }

    // report progress about every 10% of the requested endpoints
    unsigned progressInterval = num_enpoints / 10;
    if (progressInterval < 1)
        progressInterval = 1;
    unsigned numRegistered = 0;
    unsigned numFailed = 0;

    m_endpoints = new list<LoadTestEndpoint *>();
    for (unsigned i = 0; i < num_enpoints; ++i) {
        if (i > 0 && i % progressInterval == 0) {
            cout << "Progress: " << i << "/" << num_enpoints << " attempted, "
                 << numRegistered << " registered, " << numFailed << " failed" << endl;
        }

        listenPort++;   // each endpoint gets it's own TCP listenport, that will never get used, but is required for H.323 endpoint creation

        LoadTestEndpoint * ep = new LoadTestEndpoint();

        // start the H.323 listener
        H323ListenerTCP * listener = new H323ListenerTCP(*ep, interfaceAddress, listenPort);

        if (!ep->StartListener(listener)) {
            cout << "Could not open H.323 listener port on " << interfaceAddress << ":" << listener->GetListenerPort() << endl;
            delete listener;
            return;
        }

        //cout << "H.323 listening on: " << setfill(',') << ep->GetListeners() << setfill(' ') << endl;

#ifdef H323_H46018
        if (args.HasOption("h46018enable")) {
            ep->H46018Enable(true);
        } else {
            ep->H46018Enable(false);
        }
#endif
#ifdef H323_H46023
        ep->H46023Enable(false); // always disable H.460.23
#endif

        // set username for GK registration
        PString username = namePrefix + PString("_") + serverName + PString("_") + PString(i + 1);
        ep->SetLocalUserName(username);

        if (args.HasOption('p')) {
            PString gkPw = args.GetOptionString('p');
            ep->SetGatekeeperPassword(gkPw);
        }

        // process gatekeeper registration options
        //cout << "Registering with gatekeeper \"" << gkAddr << "\" ..." << flush;
        bool registered = false;
        for (unsigned attempt = 0; attempt <= REGISTRATION_RETRIES; ++attempt) {
            if (attempt > 0) {
                PTRACE(2, "Registration of " << username << " failed, retry " << attempt << " of " << REGISTRATION_RETRIES << " in " << REGISTRATION_RETRY_DELAY << " ms");
                PThread::Sleep(REGISTRATION_RETRY_DELAY);
            }
            if (ep->SetGatekeeper(gkAddr, new H323TransportUDP(*ep, interfaceAddress))) {
                registered = true;
                break;
            }
        }
        if (!registered) {
            numFailed++;
            PTRACE(1, "Error registering " << username << " with gatekeeper at " << gkAddr);
            delete ep;
            continue;
        }

        numRegistered++;
        m_endpoints->push_back(ep);

        // wait a moment to avoid flooding the gatekeeper with too many requests at once
        PThread::Sleep(delayBetweenRegistrations);
    }

    cout << "Done: " << num_enpoints << " attempted, "
         << numRegistered << " registered, " << numFailed << " failed" << endl;
	cout << "Ctrl-C to terminate" << endl;

    for (;;) {
        PThread::Sleep(100);
    }
    // never reached, endpoint unregistration is in Shutdown() which is called from UnixShutdownHandler() on SIGTERM/SIGINT/SIGQUIT
}

void LoadTestProcess::Shutdown()
{
    if (m_endpoints) {
        for (auto ep : *m_endpoints) {
            if (ep->IsRegisteredWithGatekeeper())
                ep->RemoveGatekeeper();
            delete ep;
        }
    }
	_exit(0);	// HACK: avoid destruction of global objects
}

///////////////////////////////////////////////////////////////////////////////

LoadTestEndpoint::LoadTestEndpoint()
{
    // load plugins
    LoadBaseFeatureSet();
}
