// Copyright Epic Games, Inc. All Rights Reserved.

using BuildAgent.Run;
using BuildAgent.Run.Interfaces;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace BuildAgent.Matchers.PS4
{
	[AutoRegister]
	class OrbisPubErrorMatcher : IErrorMatcher
	{
		public ErrorMatch Match(ReadOnlyLineBuffer Input)
		{
			if (Input.IsMatch(@"\[[Ee]rror\]"))
			{
				int MaxOffset = Input.MatchForwards(0, @"^\s*\[[Ee]rror\]");
				return new ErrorMatch(ErrorSeverity.Error, ErrorPriority.Low, "OrbisPubCmd", Input, 0, MaxOffset);
			}
			if (Input.IsMatch(@"\[[Ww]arn\]"))
			{
				int MaxOffset = Input.MatchForwards(0, @"^\s*\[[Ww]arn\]");
				return new ErrorMatch(ErrorSeverity.Warning, ErrorPriority.Low, "OrbisPubCmd", Input, 0, MaxOffset);
			}
			return null;
		}
	}
}
