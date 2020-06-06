// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using System.Xml;

using ORTMAPILib;

namespace PS4DevKitUtil
{
	public class SetSettings
	{

		public static bool SetTargetSetting(ITarget Target, string SettingName, string SettingValue)
		{
			string XmlSettingsDocumentDirectory = System.IO.Path.GetFullPath("Temp");
			string XmlSettingsDocumentPath = System.IO.Path.Combine(XmlSettingsDocumentDirectory, "XmlSettingsTemp.xml");

			System.IO.Directory.CreateDirectory(XmlSettingsDocumentDirectory);

			Target.GetSettingsLayout(XmlSettingsDocumentPath);
			System.Xml.XmlDocument XmlDef = new System.Xml.XmlDocument();
			XmlDef.Load(XmlSettingsDocumentPath);


			XmlNode Node = XmlDef["registry"];

			string platform = Node.Attributes["target"].Value;
			int version = Convert.ToInt32(Node.Attributes["version"].Value);

			return InternalSetTargetSetting(Target, Node, SettingName, SettingValue);
		}


		private static bool InternalSetTargetSetting(ITarget Target, XmlNode Node, string SettingName, string NewSettingValue)
		{
			bool ReturnValue = false;
			if (Node.Name.Equals("entry", StringComparison.OrdinalIgnoreCase))
			{
				string NameEnglish = Node.Attributes["eng"].Value;

				if (NameEnglish.Equals(SettingName, StringComparison.OrdinalIgnoreCase))
				{
					eSettingType SettingType = (eSettingType)Convert.ToUInt32(Node.Attributes["registrytype"].Value, 16);

					if (SettingType == eSettingType.SETTING_TYPE_STRING)
					{
						ISetting Setting = Target.AllocSetting();
						Setting.Error = 0;
						Setting.Key = Convert.ToUInt32(Node.Attributes["key"].Value, 16);
						Setting.Size = Convert.ToUInt32(Node.Attributes["registrybytesize"].Value, 16);
						Setting.Type = SettingType;

						ISetting[] Array = new ISetting[] { Setting };
						Target.GetSettings(Array, true);

						if (Setting.Value.Equals(NewSettingValue, StringComparison.InvariantCultureIgnoreCase) == false)
						{
							Setting.Value = NewSettingValue;

							Target.SetSettings(Array, true);
							ReturnValue = true;
						}
					}
					else
					{
						Console.WriteLine("Error: PS4DevKitUtil can only set settings which are strings for now");
					}
				}
			}

			foreach (XmlNode ChildNode in Node.ChildNodes)
			{
				ReturnValue = ReturnValue || InternalSetTargetSetting(Target, ChildNode, SettingName, NewSettingValue);
			}
			return ReturnValue;
		}



		private static bool PrintSetting(ORTMAPI tm, ISetting setting)
		{
			Console.WriteLine("0x{0:x}", setting.Key);

			if (setting.Error != 0)
			{
				Console.Error.WriteLine("[ERROR] {0}", tm.GetErrorDescription(setting.Error));
				Console.WriteLine();
				return false;
			}
			else
			{
				switch (setting.Type)
				{
					case eSettingType.SETTING_TYPE_STRING:
						{
							Console.WriteLine("<string>:{0}", (string)setting.Value);
						}

						break;
					case eSettingType.SETTING_TYPE_INTEGER:
						{
							Console.WriteLine("<integer>:0x{0:x}", (uint)setting.Value);
						}

						break;
					default:
						Console.WriteLine("<unknown>");
						break;
				}
			}

			Console.WriteLine();
			return true;
		}

		private static bool PrintSettings(ORTMAPI tm, ISetting[] settings)
		{
			bool bReturn = false;
			foreach (ISetting setting in settings)
			{
				bReturn |= PrintSetting(tm, setting);
			}

			return bReturn;
		}




	}
}
