//
// Created by RGAA on 8/12/2024.
//

#ifndef GAMMARAYPC_CONNECTION_H
#define GAMMARAYPC_CONNECTION_H

#include <string>

namespace tc
{

    class Connection {
    public:
        Connection();
        ~Connection();

        virtual void Start();
        virtual void Stop();

    };

}

#endif //GAMMARAYPC_CONNECTION_H
