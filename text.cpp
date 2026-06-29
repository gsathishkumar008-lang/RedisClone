#include <bits/stdc++.h>
using namespace std;

void hello()
{
    cout << "Hello\n";
}
void escape()
{
    cout << " i want to escape" << endl;
}
int sum(int x, int y){
    return x+y;
}

int main()
{
    function<void()> f = hello;
    f();
    function<void()> q = escape;
    q();
    function<int(int,int)> s = sum;
    cout<<s(2,3)<<endl;
}
