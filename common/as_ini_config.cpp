/******************************************************************
'' 文件名:as_ini_config.cpp
'' Copyright (c) .......
'' 功能描述:读取设置ini文件
'' 牵涉数据表，视图，存储过程：无
'' 包含的文件:INIConfig.h
'' 创建人:hexin
'' 日　期: 2008-8-12
'' 修改人:
'' 日　期:
'' 修改说明:
'' 版　本:Version 1.0
'' ******************************************************************/

#ifdef WIN32
#pragma warning(disable: 4786 4503)//去掉map的警告
#endif
#include <string.h>
#include "as_ini_config.h"

/***********************ini文件格式*********************************
     [SectionOne]
      Key1 = Value
      Key2 = Value
        .....
     [SectionTwo]
        .....
********************************************************************/

as_ini_config::as_ini_config()
{

}

as_ini_config::~as_ini_config()
{

}


/******************************************************************************
Function:        GetValue 
Description:    获取值
Calls:             
Called By:    
Input:            section:查询的section
                key:查询的key            
Output:            value:需要获取的值
Return:            成功返回SUCCESS 否则返回错误码
******************************************************************************/
//PCLINT注释说明：本类内部不调用该函数
long as_ini_config::GetValue (const string &section, const string &key, string &value)/*lint -e1714 暂不调用该函数，暂时保留等待扩展使用*/
{
    if ( m_SectionMap.find(section) == m_SectionMap.end() )
    {
        return INI_ERROR_NOSECTION;    
    }
  
    if ( m_SectionMap[section].find(key) == m_SectionMap[section].end() )
    {
        return INI_ERROR_NOKEY;  
    }
  
    value = m_SectionMap[section][key];
    return INI_SUCCESS;
}


/******************************************************************************
Function:        SetValue 
Description:    设置一个值
Calls:             
Called By:    
Input:            section:设置的section
                key:设置的key
                value:需要设置的值
Output:            无
Return:            无
******************************************************************************/
void as_ini_config::SetValue (const string &section, const string &key, const string &value)
{
    m_SectionMap[section][key] = value;
}

/******************************************************************************
Function:         ReadIniFile
Description:    打开配置文件
Calls:             
Called By:    
Input:            filename：要打开的文件
Output:            无
Return:            成功返回SUCCESS 否则返回错误码
******************************************************************************/
long as_ini_config::ReadIniFile (const string &filename)
{
    m_strFilePath = filename;
  
    ifstream input( filename.c_str() );
  
    if ( input.fail() )
    {
        return INI_OPENFILE_FAIL;
    }
  
    string line;
    string section;
    string left_value;
    string right_value;
    string::size_type pos;
    
    while ( !input.eof() )
    {
        (void)getline( input, line );
    
        // 去注释 
        pos = line.find('#', 0);
        if (pos != string::npos)
        {
            (void)line.erase(pos);
        }
        
        // 去掉两端空格  
        (void)line.erase(0, line.find_first_not_of("\t "));
        (void)line.erase(line.find_last_not_of("\t\r\n ") + 1);
    
        if ( line == "" )
        {
            continue;
        }  
    
    
        // 解析section 
        if ( ( line[0] == '[' ) && ( line[line.length() - 1] == ']' ) )
        {
            section = line.substr(1, line.length() - 2);
            continue;
        }
          
        // 解析等号 
        pos = line.find('=', 0);
        if ( pos != string::npos )
        {
            left_value = line.substr(0, pos);
            right_value = line.substr(pos + 1, (line.length() - pos) - 1);
      
            m_SectionMap[section][left_value] = right_value;
        }
    }
    
    input.close();
    return INI_SUCCESS;
}



/******************************************************************************
Function:        RegisterCallBack 
Description:    注册回调函数
Calls:             
Called By:    
Input:            pCallBackInfo 注册回调函数的信息
Output:            无
Return:            成功返回SUCCESS 否则返回错误码
******************************************************************************/
long as_ini_config::SaveIniFile ( const string & filename )
{
    if ( filename != "" )
    {
        m_strFilePath = filename;
    }

    ofstream output( m_strFilePath.c_str() );

    if ( output.fail() )
    {
        return INI_OPENFILE_FAIL;
    }

    for ( sections::iterator it_section = m_SectionMap.begin();
            it_section != m_SectionMap.end(); ++it_section)
    {
        if ( it_section->first != "" )
        {
            output << "[" << it_section->first << "]" << endl;
            if(output.fail())
            {
                return INI_SAVEFILE_FAIL;
            }
        }
    
        for ( items::iterator it_item = (it_section->second).begin();
                it_item != (it_section->second).end(); ++it_item )
        {
            output << it_item->first << "=" << it_item->second << endl;
            if(output.fail())
            {
                return INI_SAVEFILE_FAIL;
            }
        }
    
        output << endl;
    }
   
    return INI_SUCCESS;
}


//获取一个段所有信息
long as_ini_config::GetSection(const string & section, items & keyValueMap)
{
    sections::iterator iter = m_SectionMap.find(section);
    if ( iter == m_SectionMap.end() )
    {
        return INI_ERROR_NOSECTION;    
    }
    
    items& foundItems = iter->second;
    items::iterator Itemiter = foundItems.begin();
    items::iterator ItemEnd = foundItems.end();
    while(Itemiter != ItemEnd)
    {
        keyValueMap[Itemiter->first] = Itemiter->second;        
        ++Itemiter;
    }

    return INI_SUCCESS;
}

//将所有配置导出到某个已经存在的配置文件
//与SaveIniFile的区别是：只是修改相应值，而不会覆盖，原文件比文件多的配置项也会保留下来
long as_ini_config::ExportToFile(const string &filename)
{
    as_ini_config iniRecv;     //接收配置的文件
    long lResult = iniRecv.ReadIniFile(filename);
    if(lResult != INI_SUCCESS)
    {
        return lResult;
    }

    for(sections::iterator it_section = m_SectionMap.begin();
            it_section != m_SectionMap.end(); ++it_section)
    {
        for(items::iterator it_item = (it_section->second).begin();
                it_item != (it_section->second).end(); ++it_item )
        {
            iniRecv.SetValue(it_section->first, it_item->first, it_item->second);
        }
    }

    //保存
    lResult = iniRecv.SaveIniFile();

    return lResult;
}

//将所有配置导出到某个已经存在的配置文件
//与ExportToFile的区别是：指定的Section不需要导入
long as_ini_config::ExportToFileExceptPointed(const string &filename, 
                            unsigned long ulSectionCont, const char szSection[][INI_CONFIG_MAX_SECTION_LEN+1])
{
    as_ini_config iniRecv;     //接收配置的文件
    long lResult = iniRecv.ReadIniFile(filename);
    if(lResult != INI_SUCCESS)
    {
        return lResult;
    }

    for(sections::iterator it_section = m_SectionMap.begin();
            it_section != m_SectionMap.end(); ++it_section)
    {
        bool bPointed = false;
        //判断是否指定的不需要处理的Section
        for(unsigned long i=0; i<ulSectionCont; i++)
        {
            if(0 == strcmp(it_section->first.c_str(), szSection[i]))
            {
                bPointed = true;
                break;
            }            
        }

        //如果是指定的Section，处理下一个Section
        if(true == bPointed)
        {
            continue;
        }
        
        
        for(items::iterator it_item = (it_section->second).begin();
                it_item != (it_section->second).end(); ++it_item )
        {
            iniRecv.SetValue(it_section->first, it_item->first, it_item->second);
        }
    }

    //保存
    lResult = iniRecv.SaveIniFile();

    return lResult;
}

