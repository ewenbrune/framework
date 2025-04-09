//-----------------------------------------------------------------------------
// Copyright 2000-2022 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/* CaseOptionsGenerator.cc                                     (C) 2000-2007 */
/*                                                                           */
/* Classe générant le code des classes CaseOptions en C#.                    */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

using System;
using System.IO;
using System.Collections.Generic;
using Integer = System.Int32;

namespace Arcane.Axl
{

  /**
   * Classe générant le code des classes CaseOptions en C#.
   */
  class ExportCaseOptionsGenerator : ExportCodeGenerator
  {
    /**
      * Retourne le nom de la classe générée.
      * @return Nom de la classe générée.
      */
    public string getClassName() { return m_class_name; }

    /** Nom du module/service propriétaire du CaseOptions, par exemple hydro, photo... */
    private string m_name;

    /** Nom du namespace (null si aucun) */
    private string m_namespace_name;
    /** Version du fichier axl */
    private double m_version;
    public double Version { get { return m_version; } }
    
    /** Contenu de l'élément XML "options". */
    IList<Option> m_option_info_list;
    /** Différentes traductions de l'élément XML "name". */
    private NameTranslationsInfo m_alternative_names;
    //! \a true si le service n'est pas de type caseoption
    private bool m_not_caseoption;

    public ExportCaseOptionsGenerator(string path,
                                string output_path,
                                string name,
                                string namespace_macro_name,
                                string namespace_name,
                                double version,
                                IList<Option> option_info_list,
                                NameTranslationsInfo alternative_names,
                                bool not_caseoption)
      : base(path, output_path)
    {
      m_name = name;
      if (!String.IsNullOrEmpty(namespace_macro_name))
        Console.WriteLine("namespace-macro-name is not used in Export generator");

      m_namespace_name = namespace_name;
      m_version = version;
      m_option_info_list = option_info_list;
      m_alternative_names = alternative_names;
      m_not_caseoption = not_caseoption;

      m_class_name = "CaseOptions";
      m_class_name += m_name;
    }

    /** Génération du fichier .h */
    public override void writeFile()
    {
      TextWriter file_stream = new StringWriter();
      //WriteInfo(file_stream);
      int indent_level = 0;

      //file_stream.Write("#! TEST EXPORT\n");


      ExportClassDefinitionGenerator cdg = new ExportClassDefinitionGenerator(file_stream, indent_level);
      file_stream.Write("{\n");
      file_stream.Write("\"Name\": \"" + m_name + "\",\n");
      file_stream.Write("\"Options\": [\n");

      for(int i=0; i<m_option_info_list.Count; i++) {
        m_option_info_list[i].Accept(cdg);
        if(i!=m_option_info_list.Count-1)
          file_stream.Write(",\n");
      }

      //foreach( Option opt in m_option_info_list)
      //  opt.Accept(cdg);


      file_stream.Write("]\n}\n");
      // write file
      _writeClassFile(m_name + "_axl.json", file_stream.ToString(), true);
    }
  }
}
