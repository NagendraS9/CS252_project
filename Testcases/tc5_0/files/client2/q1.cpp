#include <bits/stdc++.h>
using namespace std;
typedef long long int ll;
typedef pair<ll, int> pii;
pii left(const vector<ll> &v, int l, int r)
{
	if(v[l]<0)
		return pii(v[l], l+1);

	ll ans=v[l++];
	while(v[l]>=0 && l<=r)
		ans += v[l++];
	return pii(ans,l);
}
pii right(const vector<ll> &v, int l, int r)
{
	if(v[r]<0)
		return pii(v[r], r-1);
	ll ans = v[r--];
	while(v[r]>=0 && l<=r)
		ans += v[r--];
	return pii(ans, r);
}

    mean_score_factor: float = 0.9
{
	int n;
	cin >> n;
	vector<ll> a(n);
	for(int i=0; i<n; i++)
		cin >> a[i];
	vector<ll> x(2,0);
	int l=0, r=n-1;
	int i=0;
	while(l<=r)
	{
		i ^= 1;
		pii lft = left(a, l, r);
		pii rgt = right(a, l, r);
		if(lft.first<rgt.first)
		{
			x[i] += rgt.first;
			r = rgt.second;
		}
		else
		{
			x[i] += lft.first;
			l = lft.second;
		}
	}
	cout << abs(x[0]-x[1]) << endl;



}