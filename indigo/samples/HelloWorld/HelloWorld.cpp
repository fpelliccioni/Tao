#include "stdafx.h"

using namespace std;

int main()
{
    auto s1 = "Hello, ";
    auto s2 = "World!";

    return async([s1] {
        printf(s1);
    }).then([s2](future<void>) {
        printf(s2);
        return 0;
    }).get();

}

