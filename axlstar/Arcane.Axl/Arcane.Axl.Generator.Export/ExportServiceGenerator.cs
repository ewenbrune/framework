//-----------------------------------------------------------------------------
// Copyright 2000-2022 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/* ExportServiceGenerator.cc                                   (C) 2000-2007 */
/*                                                                           */
/* Classe générant le code de la classe de base d'un service.                */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

using System;
using System.IO;
using System.Collections;
using Integer = System.Int32;

namespace Arcane.Axl
{
  /**
   * Classe générant le code de la classe de base d'un service. 
   */
  public class ExportServiceGenerator : ExportCodeGenerator
  {
    /** Objet stockant les informations de l'élément XML "service". */
    private ServiceInfo m_info;
    /** Générateur des fichiers "CaseOptions.h" et "CaseOptions.cc". */
    private ExportCaseOptionsGenerator m_co_generator;
    /** Nom du fichier xml contenant la description des options. */
    private string m_case_options_xml_file_name;

    public ExportServiceGenerator(string path,
                                  string output_path,
                                  ServiceInfo info)
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
                                                      m_info.NotCaseOption);
      m_case_options_xml_file_name = m_info.Name;
      m_case_options_xml_file_name += "_";
      m_case_options_xml_file_name += path;
    }

    public override void writeFile()
    {
      if (HasTests)
        Console.WriteLine("WARNING: Balise <tests> non prise en compte avec le langage C#.");

      _writeClassFile(m_info.Name + "_axl.py", null, false);

      // generation du fichier CaseOptions
      if (m_co_generator!=null)
        m_co_generator.writeFile();

      TextWriter file_stream = new StringWriter();
      WriteInfo(file_stream);
      WriteComments(file_stream);

      bool has_namespace = !string.IsNullOrEmpty(m_info.NamespaceName);

      // file act as namespace in Export
      //if (has_namespace) {
      //  file_stream.Write("namespace " + m_info.NamespaceName + "{\n");
      //}

      string parent_name = m_info.ParentName;

      // Ajoute un attribut pour indiquer au compilateur Arcane de ne pas generer de version
      // C++ de cette classe puisqu'il existe un axl.h correspondant
      file_stream.Write("public abstract class " + m_class_name + "\n");
      file_stream.Write(": " + ConvertNamespace(parent_name) + "\n");
      //for (Integer i = 0, iss = m_info.m_interfaces.Count; i < iss; ++i) {
      foreach(ServiceInfo.Interface itf in m_info.Interfaces){
        if (itf.IsInherited)
          file_stream.Write(", " + ConvertNamespace(itf.Name) + "\n");
      }
      file_stream.Write("{\n");
      file_stream.Write("  public static Arcane.ServiceInfo serviceInfoCreateFunction(string name)\n");
      file_stream.Write("  {\n");
      file_stream.Write("    Arcane.ServiceInfo si = Arcane.ServiceInfo.Create(name,(int)(Arcane.ServiceType.{0}));\n",m_info.ServiceType);
      file_stream.Write("    si.SetCaseOptionsFileName(\"" + m_case_options_xml_file_name + "\");\n");
      //for (Integer i = 0, iss = m_info.m_interfaces.Count; i < iss; ++i) {
      foreach(ServiceInfo.Interface itf in m_info.Interfaces){
        file_stream.Write("    si.AddImplementedInterface(\"" + itf.Name + "\");\n");
      }
      file_stream.Write("    si.SetAxlVersion(" + m_info.Version + ");\n");
      {
        string service_lower_case_name =  Utils.ToLowerWithDash(m_info.Name);
        file_stream.Write("    si.SetDefaultTagName(\""
                          + service_lower_case_name + "\");\n");
        foreach (DictionaryEntry de in m_info.AlternativeNames.m_names) {
          string key = (string)de.Key;
          string value = (string)de.Value;
          file_stream.Write("    si.SetTagName(\"" + key + "\",\"" + value + "\");\n");
        }
      }
      file_stream.Write("    return si;\n");
      file_stream.Write("  }\n");

      // Si la classe parent se termine par '_WrapperService', alors il s'agit d'une classe
      // générée par swig qui possède déjà une implémentation de 'CreateFactory'. Il ne faut
      // donc pas en générer une autre sinon la première ne serait pas appelée.
      if (!parent_name.EndsWith("_WrapperService")){
        // Génère une fabrique au nouveau format pour créér les instances de ce service.
        file_stream.Write("   public static Arcane.IServiceFactory2 CreateFactory(Arcane.GenericServiceFactory gsf)\n");
        file_stream.Write("   { return new Arcane.AxlGeneratedServiceFactory(gsf); }\n");
      }

      // Constructeur
      {
        file_stream.Write("  public " + m_class_name + "(Arcane.ServiceBuildInfo sbi)\n");
        file_stream.Write(": base(sbi)\n");
        file_stream.Write("{\n");

        if (m_co_generator!=null) {
          file_stream.Write("  m_options = null;\n");
        }

        _WriteVariablesConstructor(m_info.VariableInfoList,"sbi.Mesh()",file_stream);

        if (m_co_generator!=null) {
          string co_file_name = m_path;
          co_file_name += "_";
          co_file_name += m_info.Name;

          if (m_info.NotCaseOption) {
            file_stream.Write("  m_options = new " + m_co_generator.getClassName()
                              + "(sbi.SubDomain().CaseMng());\n");
          }
          else {
            file_stream.Write("  Arcane.ICaseOptions co = sbi.CaseOptions();\n");
            file_stream.Write("  if (co!=null){\n");
            file_stream.Write("    m_options = new " + m_co_generator.getClassName()
                              + "(sbi.SubDomain().CaseMng(),co);\n");
            file_stream.Write("  }\n");
          }
        }
        file_stream.Write("}\n");
      }

	  if (m_co_generator!=null) {
        file_stream.Write("  //! Options du jeu de données du module\n");
        file_stream.Write("  public " + m_co_generator.getClassName() + " Options { get { return m_options; } }\n");
        file_stream.Write("\n");
        file_stream.Write("  //! Options du jeu de données du module\n");
        file_stream.Write("  private " + m_co_generator.getClassName() + " m_options;\n\n");
      }

      _WriteVariablesDeclaration(m_info.VariableInfoList,file_stream, 0); // 0 = indent_level
      file_stream.Write("};\n");
      file_stream.Write("\n");


      WriteComments(file_stream);
      if (has_namespace) {
        file_stream.Write("}\n");
      }
      WriteComments(file_stream);

      // write file
      _writeClassFile(m_info.Name + "_axl.py", file_stream.ToString(), true);
    }
		
	private bool HasTests { get { return m_info.m_tests_info != null && m_info.m_tests_info.m_test_info_list.Count > 0; } }
  }
}
