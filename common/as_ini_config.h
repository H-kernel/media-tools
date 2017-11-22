// INIConfig.h: interface for the as_ini_config class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INICONFIG_H__C94A8432_F6BA_4C88_99BA_7C5CA0139EC7__INCLUDED_)
#define AFX_INICONFIG_H__C94A8432_F6BA_4C88_99BA_7C5CA0139EC7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <string>
#include <fstream>
#include <map>

using namespace std;



//key = value,key=value .........
typedef map<string , string> items; 
//[section] ---> key key key .........
typedef map<string , items> sections;


enum ErrorType
{
    INI_SUCCESS         =  0,
    INI_OPENFILE_FAIL   = -1,
    INI_SAVEFILE_FAIL   = -2,
    INI_ERROR_NOSECTION = -3,
    INI_ERROR_NOKEY     = -4
};

#define     INI_CONFIG_MAX_SECTION_LEN        63

#define     INI_CONFIG_LINE_SIZE_MAX          2048

class as_ini_config  
{
public:
    as_ini_config();
    virtual ~as_ini_config();
public:
    //打开ini文件
    virtual long ReadIniFile(const string &filename);
    //保存ini文件(会覆盖所有信息)
    long SaveIniFile(const string & filename = "");
    //设置Section中key对应的value
    void SetValue(const string & section, const string & key, 
        const string & value);
    //获取Section中key对应的value
    virtual long GetValue(const string & section, const string & key, 
        string & value);

    //获取一个段所有信息
    long GetSection(const string & section, items & keyValueMap);
    //将所有配置导出到某个已经存在的配置文件
    //与SaveIniFile的区别是：只是修改相应值，而不会覆盖
    long ExportToFile(const string &filename);

    //将所有配置导出到某个已经存在的配置文件
    //与ExportToFile的区别是：指定的Section不需要导入,原文件比文件多的配置项也会保留下来
    long ExportToFileExceptPointed(const string &filename, 
                            unsigned long ulSectionCont, const char szSection[][INI_CONFIG_MAX_SECTION_LEN+1]);
                            
private:

     string m_strFilePath;//打开文件路径      
     sections m_SectionMap; //保存文件内容,map

};

#endif // !defined(AFX_INICONFIG_H__C94A8432_F6BA_4C88_99BA_7C5CA0139EC7__INCLUDED_)
