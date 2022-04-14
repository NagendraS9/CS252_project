#include <bits/stdc++.h>
#include<unistd.h>
using namespace std;

void Sp(int x)
{
	while(x--)
		cout << ' ';
}
#define mili 1e3
int main()
{
	int i=0;
	while((i++)<100)
	{
		system("clear");
		//LINE 1
		Sp(i);
		if(i%2)
			cout << "       I-------I:O:I--------I" << endl;
		else
			cout << "                :O:         " << endl;
		// LINE 2
		Sp(i+2);
		if(i%2)
			cout << " | ";
		else
			cout << "\\ /";
		Sp(7);
		cout << "/----^----\\" << endl;

		// LINE 3
		Sp(i+1);
		if((i%2))
			cout << "--O--======    ";
		else
			cout << "  O  ======    ";
		Sp(5);
		cout << "()\\" << endl;


		//LINE 4
		Sp(i+2);
		if(i%2)
			cout << " | ";
		else
			cout << "/ \\";
		Sp(7);
		cout << "\\           |" << endl;

		// LINE 5
		Sp(i+13);
		cout << "\\_________/" << endl;

		// LINE 6
		Sp(i+16);
		cout << "I    I" << endl;
		usleep(75*mili);
		
	}
}