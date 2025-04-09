//-----------------------------------------------------------------------------
// Copyright 2000-2022 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
using System;
using System.IO;

namespace Arcane.Axl
{
  /**
   * Classe implémentant le Design Pattern du visiteur pour générer le code C++ de
   * déclaration des classes pour les options de type complex.
   */
  public class PythonClassDefinitionGenerator : IOptionInfoVisitor
  {
    public PythonClassDefinitionGenerator(TextWriter stream, int indent_level=0)
    {
      m_stream = stream;
      m_indent_level = indent_level;
    }
    
    public virtual void VisitComplex(ComplexOptionInfo info)
    {
      foreach(Option o in info.Options){
        o.Accept(this);
      }
      bool has_interface = !String.IsNullOrEmpty(info.InterfaceType);
      // Pour test, génère les méthodes des interfaces tout le temps
      bool always_generate_interface_method = false;
      string interface_type = String.Empty;
      if (has_interface)
        interface_type = PythonUtils.ConvertType(info.InterfaceType);
      if (info.Type == null)
        return;
      
      if (info.IsMulti) {
        string c_class_name;
        c_class_name = "CaseOption" + info.Type;
        string v_class_name;
        v_class_name = c_class_name + "Value";
        m_stream.Write(new string(' ', m_indent_level * 4) + "class " + v_class_name + "(Arcane.CaseOptionComplexValue");
        if (has_interface)
          m_stream.Write(interface_type);

        m_stream.Write("):\n");
        m_indent_level++;


        m_stream.Write(new string(' ', m_indent_level * 4) + "def __init__(self, opt: Arcane.ICaseOptionsMulti, icl: Arcane.ICaseOptionList, element: Arcane.XmlNode):\n");
        m_indent_level++;
        m_stream.Write(new string(' ', m_indent_level * 4) + "super().__init__(opt, icl, element)\n");
        PythonOptionBuilderGenerator bg = new PythonOptionBuilderGenerator(m_stream, "icl", "m_element", m_indent_level);
        info.AcceptChildren(bg);
        m_indent_level--;
        
        //m_stream.Write("    {\n");
        PythonBuilderBodyGenerator bbg = new PythonBuilderBodyGenerator(m_stream);
        info.AcceptChildren(bbg);
        //m_stream.Write("    }\n");

        m_indent_level--;

        if (has_interface || always_generate_interface_method){
          PythonInterfaceImplementationGenerator iig = new PythonInterfaceImplementationGenerator(m_stream);
          info.AcceptChildren(iig);
        }

        //if (has_interface){
        //  m_stream.Write("    public: "+interface_type+"* _interface() { return this; }\n");
        //}
        //m_stream.Write("    public:\n");
        //m_stream.Write("     const ThatClass* operator->() const { return this; }\n");
        //m_stream.Write("     const Arcane::XmlNode& element() const { return m_element; }\n");
        //m_stream.Write("    private:\n");
        //m_stream.Write("     Arcane::XmlNode m_element;\n");
        //m_stream.Write("    public:\n");
        
        //PythonVariableDefinitionGenerator vdg = new PythonVariableDefinitionGenerator(m_stream);
        //info.AcceptChildren(vdg);
        
        //m_stream.Write("    };\n");

        m_stream.Write(new string(' ', m_indent_level * 4) + "class " + c_class_name);
        m_stream.Write("(Arcane.CaseOptionsMulti):\n");
        m_indent_level++;
        m_stream.Write(new string(' ', m_indent_level * 4) + "def __init__(self, icl, s, element):\n");
        m_indent_level++;
        m_stream.Write(new string(' ', m_indent_level * 4) + "super().__init__(icl, s, element," + info.MinOccurs+","+ info.MaxOccurs + ")\n");
        m_stream.Write(new string(' ', m_indent_level * 4) + "self._values = []\n\n");

        PythonCodeGenerator.writeNameTranslations(m_stream, info.m_alternative_names,"");
        m_indent_level--;

        m_stream.Write(new string(' ', m_indent_level * 4) + "def multi_allocate(self, elist):\n");
        m_indent_level++;
        m_stream.Write(new string(' ', m_indent_level * 4) + "parent_class_name = self.__class__.__qualname__.split(\".\")[:-1]\n");
        m_stream.Write(new string(' ', m_indent_level * 4) + "parent_class_name = \".\".join(parent_class_name)\n");
        m_stream.Write(new string(' ', m_indent_level * 4) + "parent_class = globals().get(parent_class_name)\n");
        m_stream.Write(new string(' ', m_indent_level * 4) + "self._values = [parent_class."+v_class_name+"(self, self.icl, elem) for elem in elist]\n\n");
        m_indent_level--;

        m_stream.Write(new string(' ', m_indent_level * 4) + "def __getitem__(self, i):\n");
        m_indent_level++;
        m_stream.Write(new string(' ', m_indent_level * 4) + "return self._values[i]\n\n");
        m_indent_level--;

        m_stream.Write(new string(' ', m_indent_level * 4) + "def count(self):\n");
        m_indent_level++;
        m_stream.Write(new string(' ', m_indent_level * 4) + "return len(self._values)\n\n");
        m_indent_level--;

        m_stream.Write(new string(' ', m_indent_level * 4) + "def size(self):\n");
        m_indent_level++;
        m_stream.Write(new string(' ', m_indent_level * 4) + "return len(self._values)\n\n");
        m_indent_level--;

        m_stream.Write(new string(' ', m_indent_level * 4) + "def __repr__(self):\n");
        m_indent_level++;
        m_stream.Write(new string(' ', m_indent_level * 4) + "return f\"CaseOptionMaterial(count={self.count()})\"\n\n");
        m_indent_level--;

        /**
        m_stream.Write("    Arcane::ArrayView< {0}* > operator()()\n",v_class_name);
        m_stream.Write("    {\n");
        m_stream.Write("      return (*this);\n");
        m_stream.Write("    }\n");
        m_stream.Write("    void multiAllocate(const Arcane::XmlNodeList& elist){\n");
        m_stream.Write("      Arcane::Integer s = elist.size();\n");
        m_stream.Write("      " + v_class_name + "** v = 0;\n");
        m_stream.Write("      if (s!=0)\n");
        m_stream.Write("        v = new " + v_class_name + "*[s];\n");
        m_stream.Write("      _setArray(v,s);\n");
        m_stream.Write("      v = _ptr();\n");
        if (has_interface){
          m_stream.Write("      m_interfaces.resize(s);\n");
        }
        m_stream.Write("      for( Arcane::Integer i=0; i<s; ++i ){\n");
        m_stream.Write("        v[i] = new " + v_class_name);
        m_stream.Write(" (caseMng(),configList(),elist[i]);\n");
        if (has_interface){
          m_stream.Write("        m_interfaces[i] = v[i]->_interface();\n");
        }
        m_stream.Write("      }\n");
        m_stream.Write("    }\n");
        m_stream.Write("   public:\n");
        m_stream.Write("    ~" + c_class_name + "(){\n");
        m_stream.Write("      Arcane::Integer s = count();\n");
        m_stream.Write("      if (s==0)\n");
        m_stream.Write("        return;\n");
        m_stream.Write("      " + v_class_name + "** v = _ptr();\n");
        m_stream.Write("      for( Arcane::Integer i=0; i<s; ++i ){\n");
        m_stream.Write("        delete v[i];\n");
        m_stream.Write("      }\n");
        m_stream.Write("      delete[] v;");
        m_stream.Write("    }\n");
        m_stream.Write("   private:\n");
        m_stream.Write("   public:\n");
        m_stream.Write("    const " + v_class_name + "& operator[](Arcane::Integer i) const\n");
        m_stream.Write("    { return *(BaseClass::operator[](i)); }\n");
        m_stream.Write("    Arcane::Integer count() const\n");
        m_stream.Write("    { return BaseClass::size(); }\n");
        m_stream.Write("    Arcane::Integer size() const\n");
        m_stream.Write("    { return BaseClass::size(); }\n");
        if (has_interface){
          m_stream.Write("  public: Arcane::ConstArrayView< " + interface_type + "* > _interface() { return m_interfaces; }\n");
          m_stream.Write("  private: Arcane::UniqueArray< " + interface_type + "* > m_interfaces;\n");
        }
        m_stream.Write("   private:\n");
        m_stream.Write("  };\n");
        m_stream.Write('\n');
        */

        m_indent_level--;
      }

      else {
        string c_class_name;
        c_class_name = "CaseOption" + info.Type;
        m_stream.Write(new string(' ', m_indent_level * 4) + "class " + c_class_name + "(Arcane.CaseOptions");
        if (has_interface)
          m_stream.Write(", " + info.InterfaceType + "):\n");
        m_stream.Write("   public:\n");
        m_stream.Write("    " + c_class_name);
        m_stream.Write("(Arcane::ICaseMng* cm,Arcane::ICaseOptionList* icl,const Arcane::String& s,");
        m_stream.Write("const Arcane::XmlNode& element,bool is_optional=false)\n");
        m_stream.Write("    : Arcane::CaseOptions(icl,s,element,is_optional)\n");
        PythonOptionBuilderGenerator bg = new PythonOptionBuilderGenerator(m_stream, "configList()", "Arcane::XmlNode(0)");
        info.AcceptChildren(bg);
        m_stream.Write("    {\n");
        PythonCodeGenerator.writeNameTranslations(m_stream,
                                               info.m_alternative_names,
                                               "");
        PythonBuilderBodyGenerator bbg = new PythonBuilderBodyGenerator(m_stream);
        info.AcceptChildren(bbg);
        
        m_stream.Write("}\n");
        m_stream.Write("    const " + c_class_name + "& operator()() const { return *this; }\n");
        m_stream.Write("   public:\n");
        if (has_interface){
          m_stream.Write("    "+interface_type+"* _interface() { return this; }\n");
        }
        if (has_interface || always_generate_interface_method){
          PythonInterfaceImplementationGenerator iig = new PythonInterfaceImplementationGenerator(m_stream);
          info.AcceptChildren(iig);
        }
        m_stream.Write("   public:\n");
        PythonVariableDefinitionGenerator vdg = new PythonVariableDefinitionGenerator(m_stream);
        info.AcceptChildren(vdg);
        m_stream.Write("  };\n");
        m_stream.Write('\n');
      }
    }
    
    public virtual void VisitExtended(ExtendedOptionInfo info) { }
    public virtual void VisitEnumeration(EnumerationOptionInfo info) { }
    public virtual void VisitScript(ScriptOptionInfo info) { }
    public virtual void VisitSimple(SimpleOptionInfo info) { }
    public virtual void VisitServiceInstance(ServiceInstanceOptionInfo info) { }
    
    private TextWriter m_stream;
    private int m_indent_level;
  }
}