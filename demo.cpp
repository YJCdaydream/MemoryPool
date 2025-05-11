#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

using namespace std;

int main()
{
    string fname = "test.csv";
    ifstream csv_data(fname, ios::in); // 以读入方式打开文件
    if (!csv_data.is_open())
    {
        cout << "Error: opening file fail" << endl;
        exit(1);
    }
    else
    {
        string line;
        vector<string> words; // 声明一个字符串向量
        string word;
        // 读取标题行
        getline(csv_data, line);
        istringstream sin;
        // 按行读取数据
        while (getline(csv_data, line))
        {
            words.clear();
            sin.clear();
            sin.str(line);
            // 将字符串流 sin 中的字符读到字符串数组 words 中，以逗号为分隔符
            while (getline(sin, word, ','))
            {
                words.push_back(word); // 将每一格中的数据逐个 push
            }
            // 在此处可以处理 words 中的数据
        }
        csv_data.close();
        for (auto i : words)
            std::cout << i << std::endl;
    }
    return 0;
}