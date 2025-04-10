//-----------------------------------------------------------------------------
// Copyright 2000-2022 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/* JsonGenerator.cc                                            (C) 2000-2007 */
/*                                                                           */
/* Classe générant le fichier Json des options du service/module.            */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
using System.IO;
using Integer = System.Int32;

namespace Arcane.Axl
{
  /**
   * Classe générant le fichier Json des options du service/module. 
   */
  public class JsonGenerator : CodeGenerator
  {
    private ServiceOrModuleInfo m_info;

    public JsonGenerator(string path,
                                 string output_path,
                                 ServiceOrModuleInfo info)
      : base(path, output_path)
    {
      m_info = info;
    }

    protected void _writeFile(string file_name, string content, bool append)
    {
      string full_file_name = Path.Combine(m_output_path, file_name);
      TextWriter ts = null;
      if (append)
        ts = File.AppendText(full_file_name);
      else
        ts = File.CreateText(full_file_name);

      ts.Write(content);
      ts.Close();
    }

    protected void _writeOptions()
    {
      TextWriter file_stream = new StringWriter();
      int indent_level = 0;
      JsonOptionsGenerator cdg = new JsonOptionsGenerator(file_stream, indent_level);


      for (int i = 0; i < m_info.Options.Count; i++)
      {
        m_info.Options[i].Accept(cdg);
        if (i != m_info.Options.Count - 1)
          file_stream.Write(",\n");
      }

      _writeFile(m_info.Name + "_axl.json", file_stream.ToString(), true);
    }


    public override void writeFile()
    {
      StringWriter file_stream = new StringWriter();
      file_stream.Write("{\n");
      file_stream.Write("\"Name\": \"" + m_info.Name + "\",\n");
      file_stream.Write("\"Type\": \"" + m_info.TagName + "\",\n");
      file_stream.Write("\"Interface\": [");
      for (int i = 0; i < m_info.Interfaces.Count; i++)
      {
        file_stream.Write("\"" + m_info.Interfaces[i].Name + "\"");
        if (i != m_info.Interfaces.Count - 1)
          file_stream.Write(", ");
      }
      file_stream.Write("],\n");
      file_stream.Write("\"Options\": [\n");

      _writeFile(m_info.Name + "_axl.json", file_stream.ToString(), false);

      _writeOptions();

      file_stream.GetStringBuilder().Clear();
      file_stream.Write("]\n}\n");

      _writeFile(m_info.Name + "_axl.json", file_stream.ToString(), true);
    }
  }
}
