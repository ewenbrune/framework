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
  public class PythonModuleGenerator : PythonCodeGenerator
  {
    /** Objet stockant les informations de l'élément XML "module". */
    private ModuleInfo m_info;
    /** Générateur des fichiers "CaseOptions.h" et "CaseOptions.cc". */
    private PythonCaseOptionsGenerator m_co_generator;

    public PythonModuleGenerator(string path,
                                 string output_path,
                                 ModuleInfo info)
      : base(path, output_path)
    {
      m_info = info;
      m_class_name = "Arcane" + m_info.Name + "Object";
      m_co_generator = new PythonCaseOptionsGenerator(path,
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

      _writeClassFile(m_info.Name + "_axl.py", null, false);

      // generation du fichier CaseOptions
      if (m_co_generator!=null)
        m_co_generator.writeFile();

      TextWriter file_stream = new StringWriter();
      int indent_level = 0;
      WriteInfo(file_stream);
      WriteComments(file_stream);

      bool has_namespace = !  string.IsNullOrEmpty(m_info.NamespaceName);

      // file act as namespace in python
      //if (has_namespace) {
      //  file_stream.Write("namespace " + m_info.NamespaceName + "{\n");
      //  WriteComments(file_stream);
      //}

      // Ajoute un attribut pour indiquer au compilateur Arcane de ne pas generer de version
      // C++ de cette classe puisqu'il existe un axl.h correspondant
      file_stream.Write("class " + m_class_name + "(" + ConvertNamespace(m_info.ParentName) + "):\n");
      indent_level++;
      // Constructeur
      {
        file_stream.Write(new string(' ', indent_level * 4) + "def __init__(self, mb: ModuleBuildInfo):\n");
        indent_level++;

        file_stream.Write(new string(' ', indent_level * 4) +  "super().__init__(mb)\n");
        

        if (m_co_generator!=null)
          file_stream.Write(new string(' ', indent_level * 4) + "self._options = None\n");

        _WriteVariablesConstructor(m_info.VariableInfoList,"self",file_stream, indent_level);

        if (m_co_generator!=null) {
          file_stream.Write(new string(' ', indent_level * 4) + "self._options = " + m_co_generator.getClassName()
                            + "(mb.SubDomain().CaseMng())\n");
          file_stream.Write(new string(' ', indent_level * 4) + "self._options.SetCaseModule(self)\n");
        }

        // points d'entrée
        //for (Integer i = 0; i < m_info.EntryPointInfoList; i++) {
        //EntryPointInfo epi = m_info.m_entry_point_info_list[i];
        //foreach(EntryPointInfo epi in m_info.EntryPointInfoList){
        //  file_stream.Write("  _AddEntryPoint(");
        //  file_stream.Write("\"");
        //  file_stream.Write(epi.Name + "\",\n");
        //  file_stream.Write("                this.");
        //  file_stream.Write(epi.UpperMethodName);
        //  file_stream.Write(",\n                Arcane.IEntryPoint.W");
        //  file_stream.Write(ToClassName(epi.Where));
        //  file_stream.Write(",\n                Arcane.IEntryPoint.");
        //  switch (epi.Property) {
        //    case Property.PNone:
        //      file_stream.Write("PNone);\n");
        //      break;
        //    case Property.PAutoLoadBegin:
        //      file_stream.Write("PAutoLoadBegin);\n");
        //      break;
        //    case Property.PAutoLoadEnd:
        //      file_stream.Write("PAutoLoadEnd);\n");
        //      break;
        //  }
        //}
        indent_level--;
      }

	  // Points d'entrée comme méthodes abstraites
      file_stream.Write(new string(' ', indent_level * 4) + "# points d'entrée\n");
      foreach( EntryPointInfo epi in m_info.EntryPointInfoList){
        file_stream.Write(new string(' ', indent_level * 4) + "def " + epi.UpperMethodName + "(self): pass");
        file_stream.Write(" #" + epi.Name + "\n");
      }
      file_stream.Write("\n");


      if (m_co_generator!=null) {
        file_stream.Write(new string(' ', indent_level * 4) + "#! Options du jeu de données du module\n");
        file_stream.Write(new string(' ', indent_level * 4) + "@property\n");
        file_stream.Write(new string(' ', indent_level * 4) + "def options(self):\n");
        indent_level++;
        file_stream.Write(new string(' ', indent_level * 4) + "return self._options\n");
        indent_level--;
        file_stream.Write(new string(' ', indent_level * 4) + "#! Options du jeu de données du module\n");
      }

      //_WriteVariablesDeclaration(m_info.VariableInfoList,file_stream, indent_level);

      //WriteComments(file_stream);

      // file act as namespace in python
      //if (has_namespace) {
      //  file_stream.Write("}\n");
      //}
      //WriteComments(file_stream);

      // write file
      _writeClassFile(m_info.Name + "_axl.py", file_stream.ToString(), true);
    }
  }
}
