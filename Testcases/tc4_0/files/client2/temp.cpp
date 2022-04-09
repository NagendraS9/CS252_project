#include <bits/stdc++.h>
using namespace std;
bool check(int x1, int x2, int x3, int y1, int y2, int y3)
{
	return ((y2-y1)*(x3-x2)==(y3-y2)*(x2-x1));
}
int main()
{
	int n;
	cin >> n;
	int ans = n;
	vector<int> x(n), y(n);
	for(int i=0; i<n; i++)
		cin >> x[i] >> y[i];
	for(int i=0; i<n; i++)
	{
		if( check(x[i], x[(i+1)%n], x[(i+2)%n], y[i], y[(i+1)%n], y[(i+2)%n]) )
			ans--;
	}
	if(ans>2)
		cout << ans << endl;
	else
		cout << 0 << endl;
}