//-----------------------------------------------------------------------------
// Copyright 2000-2022 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/* ModuleBaseGenerator.cc                                      (C) 2000-2007 */
/*                                                                           */
/* Classe générant le code de la classe de base d'un module.                 */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
using System.IO;
using Integer = System.Int32;

namespace Arcane.Axl
{
  /**
   * Classe générant le code de la classe C# d'un module. 
   */
  public class ExportModuleGenerator : ExportCodeGenerator
  {
    /** Objet stockant les informations de l'élément XML "module". */
    private ModuleInfo m_info;
    /** Générateur des fichiers "CaseOptions.h" et "CaseOptions.cc". */
    private ExportCaseOptionsGenerator m_co_generator;

    public ExportModuleGenerator(string path,
                                 string output_path,
                                 ModuleInfo info)
      : base(path, output_path)
    {
      m_info = info;
      m_class_name = "Arcane" + m_info.Name + "Object";
      m_co_generator = new ExportCaseOptionsGenerator(path,
                                                      m_output_path,
                                                      m_info.Name,
                                                      m_info.NamespaceMacroName,
                                                      m_info.NamespaceName,
                                                      m_info.Version,
                                                      m_info.Options,
                                                      m_info.AlternativeNames,
                                                      true);
    }

    public override void writeFile()
    {
      //double version = m_info.m_version;

      _writeClassFile(m_info.Name + "_axl.json", null, false);

      // generation du fichier CaseOptions
      if (m_co_generator!=null)
        m_co_generator.writeFile();

      TextWriter file_stream = new StringWriter();

      // write file
      _writeClassFile(m_info.Name + "_axl.json", file_stream.ToString(), true);
    }
  }
}
